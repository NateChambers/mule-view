#ifndef __D2STATS__
#define __D2STATS__

#include "d2data.h"

struct D2Stat
{
  int id;
  char const* acronym;
  int descfunc;
  int descval;
  char const* descpos;
  char const* descneg;
  char const* desc2;
  int sort;
  int group;
  int groupindex;
  D2Stat* percent;

  D2Stat()
    : id(-1)
    , acronym(NULL)
    , descfunc(0)
    , descval(0)
    , descpos(NULL)
    , descneg(NULL)
    , desc2(NULL)
    , sort(0)
    , percent(NULL)
    , group(-1)
    , groupindex(0)
  {}
};
struct D2Item;

class D2StatData
{
  D2Data* data;
  char const* str_to;
  char const* str_Level;
  char const* str_Returned;
  String soxString;
  String defString;
  LocalPtr<re::Prog> prog_Level;
  LocalPtr<re::Prog> prog_StatLeft;
  LocalPtr<re::Prog> prog_StatRight;
  LocalPtr<re::Prog> prog_StatRange;
  LocalPtr<re::Prog> prog_EthSockets;
  D2Stat stats[512];
  Dictionary<D2Stat*> statDir;
  struct StatReMatch
  {
    D2Stat* stat;
    int param;
    LocalPtr<re::Prog> prog;
  };
  Array<D2MatchTree<Pair<D2Stat*, int> > > statMatchers;
  Array<Array<StatReMatch> > statReMatchers;
  void addStatMatch(int mid, char const* desc, D2Stat* stat, int param);
  D2MatchTree<int> statAppendMatch;
  int getAppendId(char const* str);

  typedef Array<Pair<int, Pair<D2Stat*, int> > > ItemProp;
  Dictionary<ItemProp> props;

  char const* skillNames[512];
  int skillClass[512];
  D2MatchTree<int> skills;
  String processRBF(Array<D2ItemStat>& list);
  String processTorch(Array<D2ItemStat>& list);
  String processAnni(Array<D2ItemStat>& list);
  bool doParse(char const* str, D2ItemStat& stat);
public:
  D2StatData(D2Data* data);

  void parse(Array<D2ItemStat>& list, char const* str);

  void ungroup(Array<D2ItemStat>& list);
  void group(Array<D2ItemStat>& list);
  String process(D2Item* item);

  void addPreset(Array<D2ItemStat>& list, char const* prop, int par, int val1, int val2);

  String format(D2ItemStat const& stat, bool abbrev = false);

  int getSkill(char const* name)
  {
    return skills.get(name);
  }
};

#endif // __D2STATS__
