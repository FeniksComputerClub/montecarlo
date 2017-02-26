#include "sys.h"
#include "debug.h"
#include "statefultask/AIEngine.h"
#include "statefultask/AIAuxiliaryThread.h"
#include "utils/GlobalObjectManager.h"
#include <iostream>
#include <chrono>
#include <atomic>
#include <random>

std::mt19937::result_type seed = 0xfe41c5;

class MonteCarlo : public AIStatefulTask {
  private:
    int m_index;
    std::mt19937 m_rand;

  protected:
    typedef AIStatefulTask direct_base_type;    // The base class of this task.

    // The different states of the task.
    enum montecarlo_state_type {
      MonteCarlo_alpha = direct_base_type::max_state,
      MonteCarlo_beta,
    };

  public:
    static state_type const max_state = MonteCarlo_beta + 1;
    MonteCarlo() : AIStatefulTask(true), m_index(0), m_rand(seed) { _MonteCarloProbe("After construction", task_state()); }

    void set_number(int n) { m_index = n; }

  protected:
    // The destructor must be protected.
    ~MonteCarlo() { }

    void initialize_impl();
    void multiplex_impl(state_type run_state);
    void abort_impl();
    void finish_impl();
    char const* state_str_impl(state_type run_state) const;
};

char const* MonteCarlo::state_str_impl(state_type run_state) const
{
  switch(run_state)
  {
    case -1: return "<not set>";
    // A complete listing of montecarlo_state_type.
    AI_CASE_RETURN(MonteCarlo_alpha);
    AI_CASE_RETURN(MonteCarlo_beta);
  }
  return "UNKNOWN";
};

void MonteCarlo::initialize_impl()
{
  set_state(MonteCarlo_alpha);
}

void MonteCarlo::abort_impl()
{
  DoutEntering(dc::statefultask, "MonteCarlo::abort_impl()");
}

void MonteCarlo::finish_impl()
{
  DoutEntering(dc::statefultask, "MonteCarlo::finish_impl()");
}

void MonteCarlo::multiplex_impl(state_type run_state)
{
  switch(run_state)
  {
    case MonteCarlo_alpha:
    {
      int randomnumber = std::uniform_int_distribution<>{1, 6}(m_rand);
      Dout(dc::notice, "randomnumber = " << randomnumber);
      bool state_changed = false;
      switch(randomnumber)
      {
        case 1:
        case 2:
          break;        // See below the switch.
        case 3:
          set_state(MonteCarlo_alpha);
          state_changed = false;
          break;
        case 4:
          set_state(MonteCarlo_beta);
          state_changed = true;
          break;
        case 5:
          advance_state(MonteCarlo_alpha);
          break;
        case 6:
          advance_state(MonteCarlo_beta);
          state_changed = true;
          break;
      }
      // We MUST call idle() or yield() if the state didn't change.
      if (!state_changed || randomnumber <= 2)
      {
        if (randomnumber == 1)
          idle();
        else
          yield(&gMainThreadEngine);
      }
      break;
    }
    case MonteCarlo_beta:
      set_state(MonteCarlo_alpha);
      break;
  }
}

int main()
{
#ifdef DEBUGGLOBAL
  GlobalObjectManager::main_entered();
#endif
  Debug(NAMESPACE_DEBUG::init());

  static_assert(!std::is_destructible<MonteCarlo>::value && std::has_virtual_destructor<MonteCarlo>::value, "Class must have a protected virtual destuctor.");

  // AIAuxiliaryThread must be manually started/stopped.
  AIAuxiliaryThread::start();

  std::mt19937 rand(seed);

  boost::intrusive_ptr<MonteCarlo> montecarlo = new MonteCarlo;
  Dout(dc::statefultask|flush_cf, "Calling montecarlo->run()");
  montecarlo->run();

  int count = 0;
  int loop_size;
  while (montecarlo->running())
  {
    if (count == 0)
      loop_size = std::uniform_int_distribution<>{2, 100}(rand);
    gMainThreadEngine.mainloop();
    std::cout << std::flush;
    if (++count >= loop_size && montecarlo->active(0))
    {
      Dout(dc::notice, "Looped " << count << " times, calling cont().");
      count = 0;
      montecarlo->cont();
    }
  }

  // Wait till AIAuxiliaryThread finished.
  AIAuxiliaryThread::stop();
}
