#include "sys.h"
#include "debug.h"
#include "statefultask/AIStatefulTask.h"
#include "statefultask/AIEngine.h"
#include "statefultask/AIAuxiliaryThread.h"

int number_of_tasks = 0;

class WaitTest : public AIStatefulTask {
  private:
    bool m_done;

  protected:
    // The base class of this task.
    using direct_base_type = AIStatefulTask;

    // The different states of the task.
    enum wait_test_state_type {
      WaitTest_start = direct_base_type::max_state,
      WaitTest_done,
    };
  public:
    static state_type const max_state = WaitTest_done + 1;    // One beyond the largest state.

  public:
    // The derived class must have a default constructor.
    WaitTest() : AIStatefulTask(true), m_done(false) { ++number_of_tasks; }

    void set_done() { m_done = true; }

  protected:
    // The destructor must be protected.
    /*virtual*/ ~WaitTest() { --number_of_tasks; }

  protected:
    /*virtual*/ char const* state_str_impl(state_type run_state) const;
    /*virtual*/ void initialize_impl();
    /*virtual*/ void multiplex_impl(state_type run_state);
};

char const* WaitTest::state_str_impl(state_type run_state) const
{
  switch(run_state)
  {
    // A complete listing of wait_test_state_type.
    AI_CASE_RETURN(WaitTest_start);
    AI_CASE_RETURN(WaitTest_done);
  }
  ASSERT(false);
  return "UNKNOWN STATE";
}

void WaitTest::initialize_impl()
{
  set_state(WaitTest_start);
  target(&gMainThreadEngine);
}

void WaitTest::multiplex_impl(state_type run_state)
{
  switch(run_state)
  {
    case WaitTest_start:
      wait_until([&](){ return m_done; }, 1, WaitTest_done);
      break;
    case WaitTest_done:
      finish();
      break;
  }
}

class Bumper : public AIStatefulTask {
  private:
    boost::intrusive_ptr<WaitTest> m_wait_test;

  protected:
    // The base class of this task.
    using direct_base_type = AIStatefulTask;

    // The different states of the task.
    enum wait_test_state_type {
      Bumper_start = direct_base_type::max_state,
      Bumper_done,
    };
  public:
    static state_type const max_state = Bumper_done + 1;    // One beyond the largest state.

  public:
    // The derived class must have a default constructor.
    Bumper(WaitTest* wait_test) : AIStatefulTask(true), m_wait_test(wait_test) { ++number_of_tasks; }

  protected:
    // The destructor must be protected.
    /*virtual*/ ~Bumper() { --number_of_tasks; }

  protected:
    /*virtual*/ char const* state_str_impl(state_type run_state) const;
    /*virtual*/ void initialize_impl();
    /*virtual*/ void multiplex_impl(state_type run_state);
};

char const* Bumper::state_str_impl(state_type run_state) const
{
  switch(run_state)
  {
    // A complete listing of wait_test_state_type.
    AI_CASE_RETURN(Bumper_start);
    AI_CASE_RETURN(Bumper_done);
  }
  ASSERT(false);
  return "UNKNOWN STATE";
}

void Bumper::initialize_impl()
{
  set_state(Bumper_start);
  target(&gAuxiliaryThreadEngine);
}

void Bumper::multiplex_impl(state_type run_state)
{
  switch(run_state)
  {
    case Bumper_start:
      Dout(dc::notice, "Sleeping for 1 second...");
      std::this_thread::sleep_for(std::chrono::seconds(1));
      set_state(Bumper_done);
      break;
    case Bumper_done:
      m_wait_test->set_done();
      m_wait_test->signal(1);
      finish();
      break;
  }
}

int main()
{
  Debug(NAMESPACE_DEBUG::init());
  gMainThreadEngine.setMaxDuration(10000.f);

  AIAuxiliaryThread::start();

  WaitTest* wait_test = new WaitTest;
  wait_test->run();

  Bumper* bumper = new Bumper(wait_test);
  bumper->run();

  for (int n = 0; n < 100000 && number_of_tasks > 0; ++n)
  {
    gMainThreadEngine.mainloop();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  AIAuxiliaryThread::stop();
}
