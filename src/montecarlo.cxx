#include "sys.h"

#ifdef CW_DEBUG_MONTECARLO
#include "statefultask/AIEngine.h"
#include "statefultask/AIAuxiliaryThread.h"
#include "statefultask/AICondition.h"
#include "utils/GlobalObjectManager.h"
#include "threadsafe/aithreadid.h"
#include "debug.h"
#include <boost/filesystem.hpp>
#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>
#include <random>
#include <set>
#include <map>

#ifdef CWDEBUG
NAMESPACE_DEBUG_CHANNELS_START
channel_ct montecarlo("MONTECARLO");
channel_ct mcstate("MCSTATE");
NAMESPACE_DEBUG_CHANNELS_END
#endif

#define MonteCarloProbe(...) MonteCarloProbeFileState(copy_state(), true, __VA_ARGS__)

namespace montecarlo {

struct FullState {
  std::string filename;
  int line;
  char const* description;
  AIStatefulTask::task_state_st task_state;
  int s1;
  char const* s1_str;
  int s2;
  char const* s2_str;
  int s3;
  char const* s3_str;

  FullState(std::string const& _filename, int _line, char const* _description, AIStatefulTask::task_state_st const& _task_state,
      int _s1, char const* _s1_str, int _s2, char const* _s2_str, int _s3, char const* _s3_str) :
    filename(_filename), line(_line), description(_description), task_state(_task_state),
    s1(_s1), s1_str(_s1_str), s2(_s2), s2_str(_s2_str), s3(_s3), s3_str(_s3_str) { }

  bool collapses(FullState const& fs) const
  {
    // If the output node starts with 'Before ' or 'Calling ' then don't collapse it.
    if (strncmp(fs.description, "Before ", 7) == 0 || strncmp(fs.description, "Calling ", 8) == 0)
      return false;
    return task_state.equivalent(fs.task_state);
  }

  void print_s123(std::ostream& os) const;
  void print_base_state(std::ostream& os) const;
  void print_task_state(std::ostream& os) const;
};

std::ostream& operator<<(std::ostream& os, FullState const& full_state)
{
  os << "{ '" << full_state.description << "' (" << full_state.filename << ':' << std::dec << full_state.line << ")";
  full_state.print_s123(os);
  os << ", ";
  full_state.print_base_state(os);
  os << ", ";
  full_state.print_task_state(os);
  return os << '}';
}

void FullState::print_s123(std::ostream& os) const
{
  if (s1 != -1) os << ' ' << s1_str;
  if (s2 != -1) os << '/' << s2_str;
  if (s3 != -1) os << '/' << s3_str;
}

void FullState::print_base_state(std::ostream& os) const
{
  os << task_state.base_state_str << '/' << task_state.run_state_str;
}

void FullState::print_task_state(std::ostream& os) const
{
  if (task_state.need_run) os << " need_run";
  if (task_state.blocked) os << " blocked";
  if (task_state.reset) os << " reset";
  if (task_state.aborted) os << " aborted";
  if (task_state.finished) os << " finished";
}

bool operator<(FullState const& fs1,  FullState const& fs2)
{
  int res = fs1.filename.compare(fs2.filename);
  if (res != 0)
    return res < 0;
  if (fs1.line != fs2.line)
    return fs1.line < fs2.line;
  if (fs1.s1 !=  fs2.s1)
    return fs1.s1 < fs2.s1;
  if (fs1.s2 !=  fs2.s2)
    return fs1.s2 < fs2.s2;
  if (fs1.s3 !=  fs2.s3)
    return fs1.s3 < fs2.s3;
  if (fs1.task_state.base_state != fs2.task_state.base_state)
    return fs1.task_state.base_state < fs2.task_state.base_state;
  if (fs1.task_state.run_state != fs2.task_state.run_state)
    return fs1.task_state.run_state < fs2.task_state.run_state;
  if (fs1.task_state.blocked != fs2.task_state.blocked)
    return fs2.task_state.blocked;
  if (fs1.task_state.reset != fs2.task_state.reset)
    return fs2.task_state.reset;
  if (fs1.task_state.need_run != fs2.task_state.need_run)
    return fs2.task_state.need_run;
  if (fs1.task_state.aborted != fs2.task_state.aborted)
    return fs2.task_state.aborted;
  if (fs1.task_state.finished != fs2.task_state.finished)
    return fs2.task_state.finished;
  return false;
}

struct Data {
  std::string name;
  int inputs;
  int outputs;

  Data(std::string const& _name) : name(_name), inputs(0), outputs(0) { }
  Data() { ASSERT(false); }
};

std::ostream& operator<<(std::ostream& os, Data const& data)
{
  return os << data.name;
}

struct Node {
  std::map<montecarlo::FullState, montecarlo::Data>::iterator me;
  std::list<Node>::iterator self;
  std::list<std::list<Node>::iterator> inputs;
  std::list<std::pair<std::list<Node>::iterator, int>> outputs;

  Node(std::map<montecarlo::FullState, montecarlo::Data>::iterator _me) : me(_me) { }

  bool operator==(std::map<montecarlo::FullState, montecarlo::Data>::iterator const& _me) const { return me == _me; }
  bool single_inout() const { return inputs.size() == 1 && outputs.size() == 1; }
  bool collapses() const { return single_inout() && outputs.begin()->first->single_inout() && me->first.collapses(outputs.begin()->first->me->first); }
  void collapse(std::string& name, std::string& desc);
};

void Node::collapse(std::string& name, std::string& desc)
{
  auto begin_node = self;
  for (;;)
  {
    auto tmp = begin_node->inputs.begin();
    if (tmp == begin_node->inputs.end() || !(*tmp)->collapses())
      break;
    begin_node = *tmp;
  }
  auto end_node = self;
  while (end_node->collapses())
    end_node = end_node->outputs.begin()->first;
  name = begin_node->me->second.name;
  if (begin_node != end_node)
    name += '_' + end_node->me->second.name;
  std::ostringstream ss;
  for (;;)
  {
    ss << begin_node->me->first.description << " (" << begin_node->me->first.filename << ':' << begin_node->me->first.line << ")\n";
    if (begin_node == end_node)
      break;
    begin_node = begin_node->outputs.begin()->first;
  }
  desc = ss.str();
}

} // namespace montecarlo

std::mt19937::result_type seed = 0xfe41c5;

int const just_running_flag                     =     0x1;
int const run_flag                              =     0x2;
int const set_state_alpha_flag                  =     0x4;
int const set_state_beta_flag                   =     0x8;
int const idle_flag                             =    0x40;
int const cont_flag                             =    0x80;
int const yield_flag                            =   0x100;
int const wait_flag                             =   0x200;
int const signalled_flag                        =   0x400;
int const abort_flag                            =   0x800;
int const finish_flag                           =  0x1000;
int const kill_flag                             =  0x2000;
int const force_kill_flag                       =  0x4000;
int const inserted_signal_flag                  = 0x20000;

class MonteCarlo : public AIStatefulTask {
  private:
    int m_index;
    std::mt19937 m_rand;
    bool m_cont_from_mainloop;
    bool m_inside_multiplex_impl;
    int m_probe_flag;
    AICondition m_condition;

    std::map<montecarlo::FullState, montecarlo::Data> m_states;
    std::map<montecarlo::FullState, montecarlo::Data>::iterator m_last_state = m_states.end();
    typedef std::map<std::pair<montecarlo::FullState, montecarlo::FullState>, int> directed_graph_type;
    directed_graph_type m_directed_graph;
    int m_transitions_count;

  protected:
    typedef AIStatefulTask direct_base_type;    // The base class of this task.

    // The different states of the task.
    enum montecarlo_state_type {
      MonteCarlo_alpha = direct_base_type::max_state,
      MonteCarlo_beta,
    };

  public:
    static state_type const max_state = MonteCarlo_beta + 1;
    MonteCarlo() : AIStatefulTask(true), m_index(0), m_rand(seed),
        m_cont_from_mainloop(false), m_inside_multiplex_impl(false),
        m_probe_flag(0), m_condition(*this), m_transitions_count(0) { MonteCarloProbe("After construction"); }

    void set_number(int n) { m_index = n; }
    void set_cont_from_mainloop(bool on) { if (!on) m_probe_flag = 0; m_cont_from_mainloop = on; if (on) m_probe_flag = cont_flag; }
    void set_inside_multiplex_impl(bool on) { m_inside_multiplex_impl = on; }
    void cont() { m_condition.signal(); }
    void write_transitions_gv();

    bool get_cont_from_mainloop() const { return m_cont_from_mainloop; }
    bool get_inside_multiplex_impl() const { return m_inside_multiplex_impl; }

  protected:
    // The destructor must be protected.
    ~MonteCarlo() { }

    void initialize_impl();
    void multiplex_impl(state_type run_state);
    void abort_impl();
    void finish_impl();
    char const* state_str_impl(state_type run_state) const;
    void probe_impl(char const* file, int file_line, bool record_state, AIStatefulTask::task_state_st state, char const* description, int s1, char const* s1_str, int s2, char const* s2_str, int s3, char const* s3_str);
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
  set_inside_multiplex_impl(true);
  switch(run_state)
  {
    case MonteCarlo_alpha:
    case MonteCarlo_beta:
    {
      int randomnumber = std::uniform_int_distribution<>{10, 49}(m_rand);
      Dout(dc::notice, "randomnumber = " << randomnumber);
      bool state_changed = false;
      switch(randomnumber / 10)
      {
        case 1:
        case 2:
          break;        // See below the switch.
        case 3:
          state_changed = run_state != MonteCarlo_alpha;
          m_probe_flag = set_state_alpha_flag;
          set_state(MonteCarlo_alpha);
          m_probe_flag = 0;
          break;
        case 4:
          state_changed = run_state != MonteCarlo_beta;
          m_probe_flag = set_state_beta_flag;
          set_state(MonteCarlo_beta);
          m_probe_flag = 0;
          break;
      }
      // We MUST call wait() or yield() if the state didn't change.
      if (!state_changed || randomnumber < 30)
      {
        if (randomnumber < 20)
        {
          m_probe_flag = idle_flag;
          wait(m_condition);
          m_probe_flag = 0;
        }
        else
        {
          m_probe_flag = yield_flag;
          yield(&gMainThreadEngine);
          m_probe_flag = 0;
        }
      }
      // Call idle() or yield() anyway in 20% of the cases after a call to set_state.
      else if (randomnumber % 10 < 2)
      {
        if (randomnumber % 10 == 0)
        {
          m_probe_flag = idle_flag;
          wait(m_condition);
          m_probe_flag = 0;
        }
        else
        {
          m_probe_flag = yield_flag;
          yield(&gMainThreadEngine);
          m_probe_flag = 0;
        }
      }
      break;
    }
  }
  set_inside_multiplex_impl(false);
}

void MonteCarlo::probe_impl(char const* file, int file_line, bool record_state, AIStatefulTask::task_state_st state, char const* description, int s1, char const* s1_str, int s2, char const* s2_str, int s3, char const* s3_str)
{
  static std::thread::id s_id;
  ASSERT(aithreadid::is_single_threaded(s_id));  // Fails if more than one thread executes this line.

  using namespace montecarlo;

  if (record_state)
  {
    boost::filesystem::path path(file);
    montecarlo::FullState full_state(path.filename().string(), /*file_line*/0, description, state, s1, s1_str, s2, s2_str, s3, s3_str);

    // Insert the new state into the std::set.
    auto it = m_states.find(full_state);
    if (it == m_states.end())
    {
      static int node_count = 0;
      std::stringstream node_name;
      node_name << "n" << node_count;
      ++node_count;
      Dout(dc::mcstate, "New node (" << node_name.str() << "): " << full_state);
      auto res = m_states.insert(std::make_pair(full_state, node_name.str()));
      it = res.first;
    }

    if (m_last_state != m_states.end())
    {
      auto res = m_directed_graph.find(std::make_pair(m_last_state->first, full_state));
      if (m_probe_flag == 0)
        m_probe_flag = just_running_flag;
      if (res == m_directed_graph.end())
      {
        m_directed_graph.insert(directed_graph_type::value_type(std::make_pair(m_last_state->first, full_state), m_probe_flag));
        ++m_transitions_count;
        Dout(dc::mcstate, m_last_state->first << "(" << m_last_state->second << ") -> " << full_state << " {" << m_transitions_count << '}');
        m_last_state->second.outputs++;
        it->second.inputs++;
        if (m_transitions_count >= 62)
          write_transitions_gv();
      }
      else
      {
        res->second |= m_probe_flag;
      }
    }

    m_last_state = it;
  }

  // Only ever insert control function calls when we're not inside a critical area of mSubState.
  // Also, do not insert a control function while we're inserting a signal().
  // And only insert control functions while running/multiplexing.
  if (!m_sub_state_locked && m_probe_flag != inserted_signal_flag && state.base_state == bs_multiplex)
  {
#ifdef CWDEBUG
    char const* file_name = strrchr(file, '/');
    file_name = file_name ? file_name + 1 : file;
#endif

    int randomnumber = std::uniform_int_distribution<>{0, 30}(m_rand);
    if (randomnumber < 10)      // Maybe insert a signal()?
    {
      // Insert a signal() once every 30 times.
      if (randomnumber == 0)
      {
        Dout(dc::statefultask, "Insertion of signal() at " << file_name << ':' << file_line);
        debug::Mark __mark;
        m_probe_flag = inserted_signal_flag;
        m_condition.signal();
        m_probe_flag = 0;
      }
    }
  }
}

void MonteCarlo::write_transitions_gv()
{
  std::list<montecarlo::Node> nodes;

  // First convert m_directed_graph to something more managable.
  for (auto&& transition : m_directed_graph)
  {
    auto from = m_states.find(transition.first.first);
    auto to = m_states.find(transition.first.second);
    int flags = transition.second;
    auto from_node = std::find(nodes.begin(), nodes.end(), from);
    if (from_node == nodes.end())
    {
      nodes.push_back(from);
      from_node = nodes.end();
      --from_node;
      nodes.back().self = from_node;
    }
    auto to_node = std::find(nodes.begin(), nodes.end(), to);
    if (to_node == nodes.end())
    {
      nodes.push_back(to);
      to_node = nodes.end();
      --to_node;
      nodes.back().self = to_node;
    }
    from_node->outputs.push_back(std::make_pair(to_node, flags));
    to_node->inputs.push_back(from_node);
  }

  std::ofstream ofile;
  ofile.open("transitions.gv");
  ofile << "strict digraph transitions {\n";
  ofile << "  node [style=filled];\n";
  for (auto node = nodes.begin(); node != nodes.end(); ++node)
  {
    if (node->collapses())
      continue;

    std::string node_name, node_description;
    node->collapse(node_name, node_description);
    ofile << "  " << node_name << " [";
    // Print node label.
    ofile << "label=\"" << node_description;
    montecarlo::FullState const& fs(node->me->first);
    fs.print_s123(ofile);
    ofile << "\n";
    fs.print_base_state(ofile);
    ofile << "\n";
    fs.print_task_state(ofile);
    ofile << "\"";

    AIStatefulTask::task_state_st const& ts(fs.task_state);
    if (node_description.find("begin loop") != std::string::npos && fs.s1 == normal_run && ts.base_state == bs_multiplex)
      ofile << ",color=red";
    if (ts.run_state == MonteCarlo_alpha)
      ofile << ",shape=box";
    else if (ts.run_state == MonteCarlo_beta)
      ofile << ",shape=hexagon";
    ofile << "];\n";

    int outgoing_flags = 0;
    for (int equal = 0; equal <= 1; ++equal)
    {
      for (auto&& out : node->outputs)
      {
        int flags = out.second;
        std::string out_name, out_description;
        out.first->collapse(out_name, out_description);
        if ((equal == 0) == (node_name == out_name))     // First time skip equal node names, second time process only the equal one.
          continue;
        ofile << "  " << node_name << " -> " << out_name;
        if (equal == 0)
          outgoing_flags |= flags;      // Collect the flags of transitions to different nodes.
        else
          flags &= ~outgoing_flags;     // Remove the flags of transitions to different nodes from the flags to the same node.
        std::string label;
        if ((flags & abort_flag))
          label += "/abort()";
        if ((flags & cont_flag))
          label += "/cont()";
        if ((flags & finish_flag))
          label += "/finish()";
        if ((flags & force_kill_flag))
          label += "/force_kill()";
        if ((flags & idle_flag))
          label += "/idle()";
        if ((flags & kill_flag))
          label += "/kill()";
        if ((flags & run_flag))
          label += "/run()";
        if ((flags & set_state_alpha_flag))
          label += "/set_state(alpha)";
        if ((flags & set_state_beta_flag))
          label += "/set_state(beta)";
        if ((flags & signalled_flag))
          label += "/signalled()";
        if ((flags & wait_flag))
          label += "/wait()";
        if ((flags & yield_flag))
          label += "/yield()";
        if ((flags & inserted_signal_flag))
          label += "/*signal()";
        bool only_inserted_flags = flags != 0 && (flags & ~inserted_signal_flag) == 0;
        ofile << " [";
        if (!label.empty())
        {
          ofile << "label=\"" << label.substr(1) << "\",fontsize=\"24\"";
          if ((flags & just_running_flag) || only_inserted_flags)
            ofile << ',';
        }
        if ((flags & just_running_flag))
          ofile << "color=green";
        else if (only_inserted_flags)
          ofile << "color=red";
        ofile << "];\n";
      }
    }
  }
  ofile << "}\n";
  ofile.close();
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
      montecarlo->set_cont_from_mainloop(true);
      montecarlo->cont();
      montecarlo->set_cont_from_mainloop(false);
    }
  }

  // Wait till AIAuxiliaryThread finished.
  AIAuxiliaryThread::stop();
}

#else
#include <iostream>
int main()
{
  std::cout << "Configure with --enable-montecarlo to let this do something." << std::endl;
}
#endif // CW_DEBUG_MONTECARLO
