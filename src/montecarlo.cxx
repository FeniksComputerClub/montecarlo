#include "sys.h"
#include "debug.h"
#include "statefultask/AIEngine.h"
#include "statefultask/AIAuxiliaryThread.h"
#include "utils/GlobalObjectManager.h"
#include <iostream>
#include <chrono>
#include <atomic>
#include <random>

class MonteCarlo : public AIStatefulTask {
  private:
    int m_index;
    std::random_device m_rd;

  protected:
    typedef AIStatefulTask direct_base_type;    // The base class of this task.

    // The different states of the task.
    enum montecarlo_state_type {
      MonteCarlo_alpha = direct_base_type::max_state,
      MonteCarlo_beta,
    };

  public:
    static state_type const max_state = MonteCarlo_beta + 1;
    MonteCarlo() : AIStatefulTask(true), m_index(0) { }

    void set_number(int n) { m_index = n; }

  protected: // The destructor must be protected.
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
    // A complete listing of montecarlo_state_type.
    AI_CASE_RETURN(MonteCarlo_alpha);
    AI_CASE_RETURN(MonteCarlo_beta);
  }
  ASSERT(false);
  return "UNKNOWN STATE";
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
      std::mt19937 gen(m_rd());
      std::uniform_int_distribution<> dis(1, 7);
      int randomnumber = dis(gen);
      std::cout << randomnumber << std::endl;
      switch(randomnumber)
      {
        case 1:
          set_state(MonteCarlo_alpha);
          break;
        case 2:
          set_state(MonteCarlo_beta);
          break;
        case 3:
          advance_state(MonteCarlo_alpha);
          break;
        case 4:
          advance_state(MonteCarlo_beta);
          break;
        case 5:
          cont();
          break;
        case 6:
        case 7:
          idle();
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

  AIAuxiliaryThread::start();

  boost::intrusive_ptr<MonteCarlo> montecarlo = new MonteCarlo;
  Dout(dc::statefultask|flush_cf, "Calling montecarlo->run()");
  montecarlo->run();

  while (montecarlo->running())
  {
    //Dout(dc::statefultask|flush_cf, "Calling gMainThreadEngine.mainloop()");
    gMainThreadEngine.mainloop();
    //Dout(dc::statefultask|flush_cf, "Returned from gMainThreadEngine.mainloop()");
    std::this_thread::sleep_for(std::chrono::microseconds(1));
  }
}
