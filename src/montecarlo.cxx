#include "sys.h"

#ifdef CW_DEBUG_MONTECARLO
#include "statefultask/AIEngine.h"
#include "statefultask/AIAuxiliaryThread.h"
#include "utils/GlobalObjectManager.h"
#include "threadsafe/aithreadid.h"
#include "debug.h"
#include <boost/filesystem.hpp>
#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <fstream>
#include <random>
#include <set>
#include <map>

#ifdef CWDEBUG
NAMESPACE_DEBUG_CHANNELS_START
channel_ct montecarlo("MONTECARLO");
channel_ct mcstate("MCSTATE");
NAMESPACE_DEBUG_CHANNELS_END
#endif

#define MonteCarloProbe(...) MonteCarloProbeFileState(copy_state(), __VA_ARGS__)

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
  void print_base_and_advance_state(std::ostream& os) const;
  void print_task_state(std::ostream& os) const;
};

std::ostream& operator<<(std::ostream& os, FullState const& full_state)
{
  os << "{ '" << full_state.description << "' (" << full_state.filename << ':' << std::dec << full_state.line << ")";
  full_state.print_s123(os);
  os << ", ";
  full_state.print_base_and_advance_state(os);
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

void FullState::print_base_and_advance_state(std::ostream& os) const
{
  os << task_state.base_state_str << '/' << task_state.run_state_str;
  if (task_state.advance_state) os << '/' << task_state.advance_state_str;
}

void FullState::print_task_state(std::ostream& os) const
{
  if (task_state.need_run) os << " need_run";
  if (task_state.idle) os << " idle";
  if (task_state.skip_idle) os << " skip_idle";
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
  if (fs1.task_state.advance_state != fs2.task_state.advance_state)
    return fs1.task_state.advance_state < fs2.task_state.advance_state;
  if (fs1.task_state.blocked != fs2.task_state.blocked)
    return fs2.task_state.blocked;
  if (fs1.task_state.reset != fs2.task_state.reset)
    return fs2.task_state.reset;
  if (fs1.task_state.need_run != fs2.task_state.need_run)
    return fs2.task_state.need_run;
  if (fs1.task_state.idle != fs2.task_state.idle)
    return fs2.task_state.idle;
  if (fs1.task_state.skip_idle != fs2.task_state.skip_idle)
    return fs2.task_state.skip_idle;
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
  std::list<std::list<Node>::iterator> outputs;

  Node(std::map<montecarlo::FullState, montecarlo::Data>::iterator _me) : me(_me) { }

  bool operator==(std::map<montecarlo::FullState, montecarlo::Data>::iterator const& _me) const { return me == _me; }
  bool single_inout() const { return inputs.size() == 1 && outputs.size() == 1; }
  bool collapses() const { return single_inout() && (*outputs.begin())->single_inout() && me->first.collapses((*outputs.begin())->me->first); }
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
    end_node = *end_node->outputs.begin();
  name = begin_node->me->second.name;
  if (begin_node != end_node)
    name += '_' + end_node->me->second.name;
  std::ostringstream ss;
  for (;;)
  {
    ss << begin_node->me->first.description << " (" << begin_node->me->first.filename << ':' << begin_node->me->first.line << ")\n";
    if (begin_node == end_node)
      break;
    begin_node = *begin_node->outputs.begin();
  }
  desc = ss.str();
}

} // namespace montecarlo

std::mt19937::result_type seed = 0xfe41c5;

class MonteCarlo : public AIStatefulTask {
  private:
    int m_index;
    std::mt19937 m_rand;
    bool m_cont_from_mainloop;
    bool m_inside_multiplex_impl;
    bool m_state_changed_and_idle_called;

    std::map<montecarlo::FullState, montecarlo::Data> m_states;
    std::map<montecarlo::FullState, montecarlo::Data>::iterator m_last_state = m_states.end();
    std::set<std::pair<montecarlo::FullState, montecarlo::FullState>> m_directed_graph;
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
        m_state_changed_and_idle_called(false), m_transitions_count(0) { MonteCarloProbe("After construction"); }

    void set_number(int n) { m_index = n; }
    void set_cont_from_mainloop(bool on) { m_cont_from_mainloop = on; }
    void set_inside_multiplex_impl(bool on) { m_inside_multiplex_impl = on; }
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
    void probe_impl(char const* file, int file_line, AIStatefulTask::task_state_st state, char const* description, int s1, char const* s1_str, int s2, char const* s2_str, int s3, char const* s3_str);
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
      int randomnumber = std::uniform_int_distribution<>{10, 69}(m_rand);
      Dout(dc::notice, "randomnumber = " << randomnumber);
      bool state_changed = false;
      switch(randomnumber / 10)
      {
        case 1:
        case 2:
          break;        // See below the switch.
        case 3:
          state_changed = run_state != MonteCarlo_alpha;
          set_state(MonteCarlo_alpha);
          break;
        case 4:
          state_changed = run_state != MonteCarlo_beta;
          set_state(MonteCarlo_beta);
          break;
        case 5:
          state_changed = run_state < MonteCarlo_alpha;
          advance_state(MonteCarlo_alpha);
          break;
        case 6:
          state_changed = run_state < MonteCarlo_beta;
          advance_state(MonteCarlo_beta);
          break;
      }
      // We MUST call idle() or yield() if the state didn't change.
      if (!state_changed || randomnumber < 30)
      {
        if (randomnumber < 20)
        {
          idle();
          if (state_changed)
            m_state_changed_and_idle_called = true;    // Tell montecarlo machinery that it is ok to insert a cont() in this case.
        }
        else
          yield(&gMainThreadEngine);
      }
      // Call idle() or yield() anyway in 20% of the cases after a call to set_state or advance_state.
      else if (randomnumber % 10 < 2)
      {
        if (randomnumber % 10 == 0)
        {
          idle();
          if (state_changed)
            m_state_changed_and_idle_called = true;    // Tell montecarlo machinery that it is ok to insert a cont() in this case.
        }
        else
          yield(&gMainThreadEngine);
      }
      break;
    }
  }
  set_inside_multiplex_impl(false);
}

void MonteCarlo::probe_impl(char const* file, int file_line, AIStatefulTask::task_state_st state, char const* description, int s1, char const* s1_str, int s2, char const* s2_str, int s3, char const* s3_str)
{
  static std::thread::id s_id;
  ASSERT(aithreadid::is_single_threaded(s_id));  // Fails if more than one thread executes this line.

  using namespace montecarlo;

  boost::filesystem::path path(file);
  montecarlo::FullState full_state(path.filename().string(), file_line, description, state, s1, s1_str, s2, s2_str, s3, s3_str);

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
    auto res = m_directed_graph.insert(std::make_pair(m_last_state->first, full_state));
    if (res.second)
    {
      ++m_transitions_count;
      Dout(dc::mcstate, m_last_state->first << "(" << m_last_state->second << ") -> " << full_state << " {" << m_transitions_count << '}');
      m_last_state->second.outputs++;
      it->second.inputs++;
      if (m_transitions_count >= 230)
        write_transitions_gv();
    }
  }

  m_last_state = it;

  // Only ever insert control function calls when we're not inside a critical area of mSubState.
  if (!m_sub_state_locked)
  {
    // It should only ever happen (by design of the task) that cont() is called when the task is idle,
    // and only exactly one cont() may be triggered in such cases. So, we should not insert a cont()
    // here when we are already triggering anything else.
    if (it->first.task_state.idle && m_state_changed_and_idle_called && m_inside_probe_impl == 1)
    {
      // Insert a cont() once every 50 times.
      int randomnumber = std::uniform_int_distribution<>{0, 50}(m_rand);
      if (randomnumber == 0)
      {
        Dout(dc::statefultask, "Insertion of cont() at " << full_state.filename << ':' << full_state.line);
        debug::Mark __mark;
        cont();
      }
    }
  }

  // If we get here and the state is not idle, then we can/should reset this because apparently we were continued again.
  if (!it->first.task_state.idle)
    m_state_changed_and_idle_called = false;
}

void MonteCarlo::write_transitions_gv()
{
  std::list<montecarlo::Node> nodes;

  // First convert m_directed_graph to something more managable.
  for (auto transition : m_directed_graph)
  {
    auto from = m_states.find(transition.first);
    auto to = m_states.find(transition.second);
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
    from_node->outputs.push_back(to_node);
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
    fs.print_base_and_advance_state(ofile);
    ofile << "\n";
    fs.print_task_state(ofile);
    ofile << "\"";

    AIStatefulTask::task_state_st const& ts(fs.task_state);
    if (ts.idle)
      ofile << ",color=green";
    else if (ts.skip_idle)
      ofile << ",color=lightblue";
    else if (node_description.find("begin loop") != std::string::npos && fs.s1 == normal_run && ts.base_state == bs_multiplex)
      ofile << ",color=red";
    if (ts.run_state == MonteCarlo_alpha)
      ofile << ",shape=box";
    else if (ts.run_state == MonteCarlo_beta)
      ofile << ",shape=hexagon";
    ofile << "];\n";

    for (auto out : node->outputs)
    {
      std::string out_name, out_description;
      out->collapse(out_name, out_description);
      std::string label;
      ofile << "  " << node_name << " -> " << out_name;
      if (out_description.find("Before abort()") != std::string::npos)
        label += "/abort()";
      if (out_description.find("Before advance_state()") != std::string::npos)
        label += std::string("/advance_state(") + out->me->first.s1_str + ")";
      if (out_description.find("Before cont()") != std::string::npos)
        label += "/cont()";
      if (out_description.find("Before finish()") != std::string::npos)
        label += "/finish()";
      if (out_description.find("Before force_kill()") != std::string::npos)
        label += "/force_kill()";
      if (out_description.find("Before idle()") != std::string::npos)
        label += "/idle()";
      if (out_description.find("Before kill()") != std::string::npos)
        label += "/kill()";
      if (out_description.find("Before run()") != std::string::npos)
        label += "/run()";
      if (out_description.find("Before set_state()") != std::string::npos)
        label += std::string("/set_state(") + out->me->first.s1_str + ")";
      if (out_description.find("Before signalled()") != std::string::npos)
        label += "/signalled()";
      if (out_description.find("Before wait()") != std::string::npos)
        label += "/wait()";
      if (out_description.find("Calling yield()") != std::string::npos)
        label += "/yield()";
      if (!label.empty())
        ofile << " [label=\"" << label.substr(1) << "\",fontsize=\"24\"]";
      ofile << ";\n";
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

#endif // CW_DEBUG_MONTECARLO
