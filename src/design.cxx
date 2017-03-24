#include "sys.h"
#include "debug.h"
#include "utils/MultiLoop.h"
#include "statefultask/AIStatefulTask.h"
#include "statefultask/AIEngine.h"

//===========================================================================
// Task

struct Task : public AIStatefulTask {
  private:
    bool m_do_finish;

  protected:
    // The base class of this task.
    typedef AIStatefulTask direct_base_type;

    // The different states of the task.
    enum design_state_type {
      Task_start = direct_base_type::max_state,
      Task_done,
    };
  public:
    static state_type const max_state = Task_done + 1;    // One beyond the largest state.

  public:
    // The derived class must have a default constructor.
    Task();

  protected:
    // The destructor must be protected.
    /*virtual*/ ~Task();

  protected:
    // The following virtual functions must be implemented:

    // Handle initializing the object.
    /*virtual*/ void initialize_impl();

    // Handle mRunState.
    /*virtual*/ void multiplex_impl(state_type run_state);

    // Handle aborting from current bs_multiplex state (the default AIStatefulTask::abort_impl() does nothing).
    /*virtual*/ void abort_impl() { }

    // Handle cleaning up from initialization (or post abort) state (the default AIStatefulTask::finish_impl() does nothing).
    /*virtual*/ void finish_impl() { }

    // Return human readable string for run_state.
    /*virtual*/ char const* state_str_impl(state_type run_state) const;

  public:
    // Cause task to finish.
    void do_finish() { m_do_finish = true; signal(1); gMainThreadEngine.mainloop(); }
};

Task::Task() : AIStatefulTask(true), m_do_finish(false)
{
}

Task::~Task()
{
}

char const* Task::state_str_impl(state_type run_state) const
{
  switch(run_state)
  {
    AI_CASE_RETURN(Task_start);
    AI_CASE_RETURN(Task_done);
  }
  return "UNKNOWN";
}

void Task::initialize_impl()
{
  set_state(Task_start);
}

void Task::multiplex_impl(state_type run_state)
{
  switch(run_state)
  {
    case Task_start:
      if (!m_do_finish)
      {
        wait(1);
        break;
      }
      set_state(Task_done);
      /*fall-through*/
    case Task_done:
      finish();
      break;
  }
}

//===========================================================================
//

class Inserter : public MultiLoop {
  private:
    int m_i;                    // Runs over elements of tasks when calling add().
    int m_N;                    // The number of tasks / for loops.
    int m_M;                    // The number of times insert will be called per inner loop.
    std::vector<boost::intrusive_ptr<Task>*> tasks;   // N is size of this vector.

  public:
    Inserter(int n, int m) : MultiLoop(n), m_i(0), m_N(n), m_M(m), tasks(n) { }
    void add(boost::intrusive_ptr<Task>* task) { ASSERT(m_i < m_N); tasks[m_i++] = task; }
    int insert(int m) const;
    int number_of_insertions_at(int m);
};

int Inserter::insert(int m) const
{
  int finished = 0;
  for (int task = 0; task < m_N; ++task)
    if ((*this)[task] == m)
    {
      (*tasks[task])->do_finish();
      ++finished;
    }
  return finished;
}

int Inserter::number_of_insertions_at(int m)
{
  int count = 0;
  for (int task = 0; task < m_N; ++task)
    count += ((*this)[task] == m) ? 1 : 0;
  return count;
}

//===========================================================================
// TestSuite

struct TestSuite final : public Task {
  protected:
    // The base class of this task.
    typedef Task direct_base_type;

    // The different states of the task.
    enum design_state_type {
      Test1 = direct_base_type::max_state,
      Test2,
      Test3,
      Test4,
      Test5,
      Test6,
      Test7,
      Test8
    };

  public:
    static state_type const max_state = Test8 + 1;    // One beyond the largest state.

    void test1();
    void test2();
    void test3();
    void test4();
    void test5();
    void test6();
    void test7();
    void test8();

  private:
    int m_run_test;

  public:
    boost::intrusive_ptr<Task> task1;
    boost::intrusive_ptr<Task> task2;
    boost::intrusive_ptr<Task> task3;
    boost::intrusive_ptr<Task> task4;

    void run_test(int test);

    TestSuite() : task1(new Task), task2(new Task), task3(new Task), task4(new Task) { }

  protected:
    /*virtual*/ ~TestSuite() { task1.reset(); task2.reset(); task3.reset(); task4.reset(); }

    /*virtual*/ void initialize_impl();
    /*virtual*/ void multiplex_impl(state_type run_state);
    /*virtual*/ char const* state_str_impl(state_type run_state) const;
};

char const* TestSuite::state_str_impl(state_type run_state) const
{
  switch(run_state)
  {
    AI_CASE_RETURN(Test1);
    AI_CASE_RETURN(Test2);
    AI_CASE_RETURN(Test3);
    AI_CASE_RETURN(Test4);
    AI_CASE_RETURN(Test5);
    AI_CASE_RETURN(Test6);
    AI_CASE_RETURN(Test7);
    AI_CASE_RETURN(Test8);
  }
  ASSERT(run_state < direct_base_type::max_state);
  return direct_base_type::state_str_impl(run_state);
}

int main()
{
  Debug(NAMESPACE_DEBUG::init());

  boost::intrusive_ptr<TestSuite> testsuite;

  testsuite = new TestSuite; testsuite->run_test(1); testsuite.reset();
  testsuite = new TestSuite; testsuite->run_test(2); testsuite.reset();
  testsuite = new TestSuite; testsuite->run_test(3); testsuite.reset();
  testsuite = new TestSuite; testsuite->run_test(4); testsuite.reset();
  testsuite = new TestSuite; testsuite->run_test(5); testsuite.reset();
  testsuite = new TestSuite; testsuite->run_test(6); testsuite.reset();
  testsuite = new TestSuite; testsuite->run_test(7); testsuite.reset();
  testsuite = new TestSuite; testsuite->run_test(8); testsuite.reset();
}

void TestSuite::run_test(int test)
{
  m_run_test = test;
  run();
}

void TestSuite::initialize_impl()
{
  set_state(Test1 + m_run_test - 1);
}

void TestSuite::multiplex_impl(state_type run_state)
{
  switch(run_state)
  {
    case Test1:
      test1();
      break;
    case Test2:
      test2();
      break;
    case Test3:
      test3();
      break;
    case Test4:
      test4();
      break;
    case Test5:
      test5();
      break;
    case Test6:
      test6();
      break;
    case Test7:
      test7();
      break;
    case Test8:
      test8();
      break;
  }
  abort();
}

//===========================================================================
// The actual tests.

void TestSuite::test1()
{
  DoutEntering(dc::notice, "TestSuite::test1()");
  ASSERT(running());            // We are running.

  task1->run(this, 2);          // Start one task.
  gMainThreadEngine.mainloop();

  ASSERT(!*task1);              // Task 1 is not finished (is still going to call the callback).
  ASSERT(!waiting());           // We are not idle.

  wait(2);                      // Go idle.
  ASSERT(waiting());            // We are idle.

  task1->do_finish();           // Task 1 finishes.
  ASSERT(*task1);               // Task 1 is finished.
  ASSERT(!waiting());           // We are again not idle.
}

void TestSuite::test2()
{
  DoutEntering(dc::notice, "TestSuite::test2()");
  ASSERT(running());            // We are running.

  task1->run(this, 2);          // Start one task.
  gMainThreadEngine.mainloop();

  task1->do_finish();           // Task 1 finishes.
  ASSERT(*task1);               // Task 1 is finished.
  ASSERT(running() && !waiting());      // We are running.

  wait(2);                       // Go idle.
  ASSERT(running() && !waiting());      // Still running.

  ASSERT(*task1);               // Task 1 is finished.
}

void TestSuite::test3()
{
  DoutEntering(dc::notice, "TestSuite::test3()");
  task1->run(this, 2);          // Start two tasks.
  task2->run(this, 4);
  gMainThreadEngine.mainloop();

  ASSERT(running() && !waiting());
  ASSERT(!*task1 && !*task2);   // Neither task is finished.

  wait(2);                      // Go idle.
  ASSERT(waiting());

  task1->do_finish();           // Task 1 finishes.
  ASSERT(*task1 && !*task2);    // Task 1 is finished, task 2 isn't.

  wait(4);                      // Go idle.
  ASSERT(waiting());

  task2->do_finish();           // Task 2 finishes.
  ASSERT(*task1 && *task2);     // Both tasks finished.
}

void TestSuite::test4()
{
  DoutEntering(dc::notice, "TestSuite::test4()");
  task1->run(this, 2);          // Start two tasks.
  task2->run(this, 4);
  gMainThreadEngine.mainloop();

  wait(2);                      // Go idle.
  ASSERT(waiting());

  task1->do_finish();           // Task 1 finishes.
  task2->do_finish();           // Task 2 finishes.

  wait(4);                      // Go idle.
  ASSERT(running() && !waiting());
}

void TestSuite::test5()
{
  DoutEntering(dc::notice, "TestSuite::test5()");
  task1->run(this, 2);          // Start two tasks.
  task2->run(this, 4);
  gMainThreadEngine.mainloop();

  task1->do_finish();           // Task 1 finishes.

  wait(2);                      // Go idle.
  ASSERT(running() && !waiting());

  wait(4);                      // Go idle.
  ASSERT(waiting());

  task2->do_finish();           // Task 2 finishes.
}

void TestSuite::test6()
{
  DoutEntering(dc::notice, "TestSuite::test6()");
  task1->run(this, 2);          // Start two tasks.
  task2->run(this, 4);
  gMainThreadEngine.mainloop();

  task1->do_finish();           // Task 1 finishes.

  wait(2);                      // Go idle.
  ASSERT(running() && !waiting());

  task2->do_finish();           // Task 2 finishes.

  wait(4);                      // Go idle.
  ASSERT(running() && !waiting());
}

void TestSuite::test7()
{
  DoutEntering(dc::notice, "TestSuite::test7()");
  task1->run(this, 2);          // Start two tasks.
  task2->run(this, 4);
  gMainThreadEngine.mainloop();

  task1->do_finish();           // Task 1 finishes.
  task2->do_finish();           // Task 2 finishes.

  wait(2);                      // Go idle.
  ASSERT(running() && !waiting());

  ASSERT(*task1 && *task2);     // Both tasks finished.

  wait(4);                      // Go idle.
  ASSERT(running() && !waiting());

  wait(2);
  ASSERT(waiting());            // Calling wait() twice on a row always causes us to go idle!
}

void TestSuite::test8()
{
  DoutEntering(dc::notice, "TestSuite::test8()");
  gMainThreadEngine.setMaxDuration(10000.f);

  int count = 0;
  int loops = 0;
  Inserter ml(4, 25);           // 4 tasks, 7 insertion points.
  ml.add(&task1);
  ml.add(&task2);
  ml.add(&task3);
  ml.add(&task4);
  for (; !ml.finished(); ml.next_loop())
    for (; ml() < 25; ++ml)
      if (ml.inner_loop())
      {
        // Reset the tasks.
        task1 = new Task;
        task2 = new Task;
        task3 = new Task;
        task4 = new Task;

        ++loops;
        int wait_calls = 0;

        // Reset main task.
        signal(-1);
        signal(-1);
        wait(-1);
        // Main task is running and not idle().
        ASSERT(running() && !waiting());

        task1->run(this, 2);  // Start three tasks that signal 2 when they finish.
        task2->run(this, 2);
        task3->run(this, 2);
        task4->run(this, 2);
        gMainThreadEngine.mainloop();

        int n = 0;
        bool nonsense = false;
        int finished = ml.insert(n++);
        for(;;)
        {
          bool task1t1 = *task1;
          finished += ml.insert(n++);
          bool task2t1 = *task2;
          finished += ml.insert(n++);
          bool task3t1 = *task3;
          finished += ml.insert(n++);
          bool task2t2 = *task2;
          finished += ml.insert(n++);
          bool task3t2 = *task3;
          finished += ml.insert(n++);
          bool task4t2 = *task4;
          finished += ml.insert(n++);
          if ((task1t1 && task2t1 && task3t1) || (task2t2 && task3t2 && task4t2))       // We need either task1, 2 and 3 to have finished, or 2, 3 and 4.
            break;
          finished += ml.insert(n++);
          wait(2);             // Go idle until one or more tasks are finished.
          ++wait_calls;
          if ((*task1 && *task2 && *task3) || (*task2 && *task3 && *task4))
            break;
          ASSERT(wait_calls <= finished + 1);
          ASSERT((running() && !waiting()) || finished < 4);
          if (waiting() && ml.number_of_insertions_at(n) == 0)
          {
            nonsense = true;
            break;              // The test is nonsense.
          }
          finished += ml.insert(n++);
          ASSERT(running() && !waiting()); // We should only continue to run after a wait when we're really running ;).
        }
        if (nonsense)
          continue;
        ++count;
        for(;;)
        {
          int done = 0;
          done += *task1 ? 1 : 0;
          done += *task2 ? 1 : 0;
          done += *task3 ? 1 : 0;
          done += *task4 ? 1 : 0;
          while (running() && !waiting())
          {
            wait(2);
            ++wait_calls;
          }
          if (done == 4)
            break;
          ASSERT(n < 25);
          finished += ml.insert(n++);
        }
        ASSERT(finished == 4);
        ASSERT(wait_calls <= 5);
      }
  Dout(dc::notice, "count = " << count << "; loops = " << loops);
}
