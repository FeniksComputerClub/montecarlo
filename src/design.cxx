#include "sys.h"
#include "debug.h"
#include "utils/MultiLoop.h"

struct ConditionVariable;

//===========================================================================
// Task

struct Task {
  Task* m_parent;
  ConditionVariable* m_cv;
  int m_idle;
  bool m_finished;

  Task() { reset(); }
  void reset() { m_parent = nullptr; m_cv = nullptr; m_idle = 0; m_finished = false; }

  void run(Task* parent);
  void run(ConditionVariable& cv);
  void finish();

  void idle();
  void cont();
  void wait(ConditionVariable& cv);

  bool running() const;
  typedef Task* const* bool_type;
  operator bool_type() const;
};

void Task::run(Task* parent)
{
  m_parent = parent;
}

void Task::run(ConditionVariable& cv)
{
  m_cv = &cv;
}

Task::operator bool_type() const
{
  // If this returns false then run() was called and finish() wasn't called yet.
  return (!(!m_finished && (m_parent || m_cv))) ? &m_parent : 0;
}

void Task::idle()
{
  // idle() may only be called while we are running (because it may only be called from multiplex_impl()).
  ASSERT(m_idle >= 0);
  --m_idle;
}

void Task::cont()
{
  ++m_idle;
}

bool Task::running() const
{
  return m_idle >= 0;
}

//===========================================================================
//

class ConditionVariable {
  private:
    int m_idle;
  public:
    ConditionVariable() : m_idle(0) { }
    void idle();
    void cont();
    bool running() const;
};

void Task::finish()
{
  m_finished = true;
  if (m_parent)
  {
    m_parent->cont();
    // After a child tasks finishes, we always need the parent to run (in this case; we don't abort).
    ASSERT(m_parent->running());
  }
  else if (m_cv)
  {
    m_cv->cont();
  }
}

void Task::wait(ConditionVariable& cv)
{
  cv.idle();
}

void ConditionVariable::idle()
{
  ASSERT(m_idle >= 0);
  --m_idle;
}

void ConditionVariable::cont()
{
  ++m_idle;
}

bool ConditionVariable::running() const
{
  return m_idle >= 0;
}

//===========================================================================
//

class Inserter : public MultiLoop {
  private:
    int m_i;                    // Runs over elements of tasks when calling add().
    int m_N;                    // The number of tasks / for loops.
    int m_M;                    // The number of times insert will be called per inner loop.
    std::vector<Task*> tasks;   // N is size of this vector.

  public:
    Inserter(int n, int m) : MultiLoop(n), m_i(0), m_N(n), m_M(m), tasks(n) { }
    void add(Task& task) { ASSERT(m_i < m_N); tasks[m_i++] = &task; }
    void insert(int m) const;
    int number_of_insertions_at(int m);
};

void Inserter::insert(int m) const
{
  for (int task = 0; task < m_N; ++task)
    if ((*this)[task] == m)
      tasks[task]->finish();
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

struct TestSuite : public Task {
  Task task1;
  Task task2;
  Task task3;
  Task task4;
  void reset() { Task::reset(); task1.reset(); task2.reset(); task3.reset(); task4.reset(); }
  void idle(bool all_done = false) { ASSERT(all_done || !(task1 && task2 && task3 && task4)); Task::idle(); }
  void test1();
  void test2();
  void test3();
  void test4();
  void test5();
  void test6();
  void test7();
  void test8();
};

int main()
{
  Debug(NAMESPACE_DEBUG::init());

  TestSuite testsuite;

  testsuite.reset(); testsuite.test1();
  testsuite.reset(); testsuite.test2();
  testsuite.reset(); testsuite.test3();
  testsuite.reset(); testsuite.test4();
  testsuite.reset(); testsuite.test5();
  testsuite.reset(); testsuite.test6();
  testsuite.reset(); testsuite.test7();
  testsuite.reset(); testsuite.test8();
}

//===========================================================================
// The actual tests.

void TestSuite::test1()
{
  task1.run(this);              // Start one task.
  ASSERT(!task1);               // task1 is not finished (is still going to call the callback).
  ASSERT(running());            // We are running.

  idle();                       // Go idle.
  ASSERT(!running());           // We are not running (we're idle).

  task1.finish();               // Task finishes.
  ASSERT(task1);                // task1 is finished.
  ASSERT(running());            // We are running again.
}

void TestSuite::test2()
{
  task1.run(this);              // Start one task.
  task1.finish();               // Task finishes.
  ASSERT(task1);                // task1 is finished.
  ASSERT(running());            // We are running.

  idle(true);                   // Go idle (true = tell testsuite that all tasks finished).
  ASSERT(running());

  ASSERT(task1);                // task1 is finished.
}

void TestSuite::test3()
{
  task1.run(this);              // Start two tasks.
  task2.run(this);
  ASSERT(running());
  ASSERT(!task1 && !task2);     // Neither task is finished.

  idle();                       // Go idle.
  ASSERT(!running());

  task1.finish();               // Task 1 finishes.
  ASSERT(task1 && !task2);      // Task 1 is finished, task 2 isn't.

  idle();                       // Go idle.
  ASSERT(!running());

  task2.finish();               // Task 2 finishes.
  ASSERT(task1 && task2);       // Both tasks finished.
}

void TestSuite::test4()
{
  task1.run(this);              // Start two tasks.
  task2.run(this);

  idle();                       // Go idle.
  ASSERT(!running());

  task1.finish();               // Task 1 finishes.
  task2.finish();               // Task 2 finishes.

  idle(true);                   // Go idle.
  ASSERT(running());
}

void TestSuite::test5()
{
  task1.run(this);              // Start two tasks.
  task2.run(this);

  task1.finish();               // Task 1 finishes.

  idle();                       // Go idle.
  ASSERT(running());

  idle();                       // Go idle.
  ASSERT(!running());

  task2.finish();               // Task 2 finishes.
}

void TestSuite::test6()
{
  task1.run(this);              // Start two tasks.
  task2.run(this);

  task1.finish();               // Task 1 finishes.

  idle();                       // Go idle.
  ASSERT(running());

  task2.finish();               // Task 2 finishes.

  idle(true);                   // Go idle.
  ASSERT(running());
}

void TestSuite::test7()
{
  task1.run(this);              // Start two tasks.
  task2.run(this);

  task1.finish();               // Task 1 finishes.
  task2.finish();               // Task 2 finishes.

  idle(true);                   // Go idle.
  ASSERT(running());

  idle(true);                   // Go idle.
  ASSERT(running());
}

void TestSuite::test8()
{
  int count = 0;
  int loops = 0;
  Inserter ml(4, 7);            // 4 tasks, 7 insertion points.
  ml.add(task1);
  ml.add(task2);
  ml.add(task3);
  ml.add(task4);
  for (; !ml.finished(); ml.next_loop())
    for (; ml() < 7; ++ml)
      if (ml.inner_loop())
      {
        ++loops;
        reset();
        int wait_calls = 0;

        ConditionVariable cv;         // A condition variable.
        task1.run(cv);                // Start three tasks that signal the cv when they finish.
        task2.run(cv);
        task3.run(cv);
        task4.run(cv);

        int n = 0;
        bool nonsense = false;
        ml.insert(n++);
        while (!((task1 && task2 && task3) || (task2 && task3 && task4)))       // We need either task1, 2 and 3 to have finished, or 2, 3 and 4.
        {
          ml.insert(n++);
          wait(cv);             // Go idle until one or more tasks are finished.
          ++wait_calls;
          if (!cv.running() && ml.number_of_insertions_at(n) == 0)
          {
            nonsense = true;
            break;           // The test is nonsense.
          }
          ml.insert(n++);
          ASSERT(cv.running()); // We should only continue to run after a wait when we're really running ;).
        }
        if (nonsense)
          continue;
        wait(cv);
        ++wait_calls;
        count += cv.running() ? 1 : 0;
        for(;;)
        {
          int finished = 0;
          finished += task1 ? 1 : 0;
          finished += task2 ? 1 : 0;
          finished += task3 ? 1 : 0;
          finished += task4 ? 1 : 0;
          while (cv.running())
          {
            wait(cv);
            ++wait_calls;
          }
          if (finished == 4)
            break;
          ASSERT(n < 7);
          ml.insert(n++);
        }
        ASSERT(wait_calls == 5);
      }
  Dout(dc::notice, "count = " << count << "; loops = " << loops);
}
