#include "d2stats.h"
#include "d2mule.h"

#define GROUP_BASE    361

extern char const* statAcronyms[512];
int D2StatData::getAppendId(char const* str)
{
  if (!str) return 0;
  int id = statAppendMatch.geteq(str);
  if (!id)
  {
    statAppendMatch.add(str, id = statMatchers.length());
    statMatchers.push();
    statReMatchers.push();
  }
  return id;
}
static int clsString[] = {3525, 3528, 3527, 3526, 3529, 21205, 21206};
static int tabString[] = {
  10056, 10055, 10054,
  10068, 10067, 10066,
  10062, 10061, 10060,
  10059, 10058, 10057,
  10065, 10064, 10063,
  10069, 10070, 10071,
  10072, 10073, 10074
};

static re::Prog* compile(char const* desc)
{
  String prog;
  for (int i = 0; desc[i]; i++)
  {
    switch (desc[i])
    {
    case '%':
    {
      i++;
      switch (desc[i])
      {
      case '%':
        prog += '%';
        break;
      case 'd':
        prog += "(-?[0-9]+)";
        break;
      case 's':
        prog += "(.*)";
        break;
      }
    }
      break;
    case '\\':
    case '*':
    case '+':
    case '?':
    case '|':
    case '(':
    case ')':
    case '{':
    case '}':
    case '^':
    case '$':
    case '.':
    case '[':
      prog += '\\';
      prog += desc[i];
      break;
    case '\n':
      break;
    default:
      prog += desc[i];
    }
  }
  return new re::Prog(prog);
}

D2StatData::D2StatData(D2Data* _data)
  : data(_data)
{
  str_to = data->getLocalizedString(3464);
  str_Level = data->getLocalizedString(4185);
  str_Returned = data->getLocalizedString(3013);

  D2Excel table(TempFile(data->getFile("data\\global\\excel\\ItemStatCost.txt")));
  int col0 = table.colByName("descpriority");
  statMatchers.push();
  statReMatchers.push();
  for (int i = 0; i < table.rows(); i++)
  {
    char const* name = table.value(i, 0);
    int id = atoi(table.value(i, 1));
    if (!*name) continue;
    statDir.set(name, &stats[id]);

    stats[id].id = id;
    stats[id].acronym = statAcronyms[id];
    stats[id].sort = atoi(table.value(i, col0));
    stats[id].descfunc = atoi(table.value(i, col0 + 1));
    stats[id].descval = atoi(table.value(i, col0 + 2));
    stats[id].descpos = data->getLocalizedString(table.value(i, col0 + 3));
    stats[id].descneg = data->getLocalizedString(table.value(i, col0 + 4));
    stats[id].desc2 = data->getLocalizedString(table.value(i, col0 + 5));
    stats[id].group = atoi(table.value(i, col0 + 6)) - 1;
    if (stats[id].group >= 0)
      stats[id].groupindex = 1;

    if (stats[id].descfunc == 17 || stats[id].descfunc == 18)
      continue;
    if ((id >= 305 && id <= 308) || id == 23 || id == 24)
      continue;
    if (id == 194)
    {
      soxString = String::format("%s (%%d)", data->getLocalizedString(3453));
      stats[id].descpos = stats[id].descneg = soxString;
      stats[id].descfunc = 11;
    }

    if (id == 83)
    {
      for (int j = 0; j < sizeof clsString / sizeof clsString[0]; j++)
        addStatMatch(0, data->getLocalizedString(clsString[j]), &stats[id], j);
    }
    else if (id == 188)
    {
      for (int j = 0; j < sizeof tabString / sizeof tabString[0]; j++)
      {
        int mid = getAppendId(data->getLocalizedString(10917 + j / 3));
        addStatMatch(mid, data->getLocalizedString(tabString[j]), &stats[id], j);
      }
    }
    else
    {
      int mid = getAppendId(stats[i].desc2);
      if (stats[i].descpos)
        addStatMatch(mid, stats[i].descpos, &stats[i], 0);
      if (stats[i].descneg && (!stats[i].descpos || strcmp(stats[i].descpos, stats[i].descneg)))
        addStatMatch(mid, stats[i].descneg, &stats[i], 0);
    }
  }

  int grpString[] = {22745,  3461, 10977, 10024,  3623,  3613,  3615,  3617,  3619, 10034, 10023};
  int grpFunc[]   = {    3,     3,     1,    19,    33,    33,    33,    33,    33,    34,     4};
  int grpVal[]    = {    0,     2,     1,     0,     0,     0,     0,     0,     0,     0,     1};
  int grpSort[]   = {    0,   200,    61,    34,   126,   101,    95,    98,   103,    91,   129};
  for (int i = 0; i < 11; i++)
  {
    int id = GROUP_BASE + i - 2;
    stats[id].id = id;
    stats[id].acronym = statAcronyms[id];
    stats[id].sort = grpSort[i];
    stats[id].descfunc = grpFunc[i];
    stats[id].descval = grpVal[i];
    stats[id].descpos = data->getLocalizedString(grpString[i]);
    stats[id].descneg = data->getLocalizedString(grpString[i]);

    addStatMatch(0, stats[id].descpos, &stats[id], 0);
  }

  stats[21].group = 2; stats[21].groupindex = 1; stats[22].group = 2; stats[22].groupindex = 2;
  stats[48].group = 3; stats[48].groupindex = 1; stats[49].group = 3; stats[49].groupindex = 2;
  stats[50].group = 5; stats[50].groupindex = 1; stats[51].group = 5; stats[51].groupindex = 2;
  stats[54].group = 4; stats[54].groupindex = 1; stats[55].group = 4; stats[55].groupindex = 2;
    stats[56].group = 4; stats[56].groupindex = 0;
  stats[52].group = 6; stats[52].groupindex = 1; stats[53].group = 6; stats[53].groupindex = 2;
  stats[57].group = 7; stats[57].groupindex = 1; stats[58].group = 7; stats[58].groupindex = 2;
    stats[59].group = 7; stats[59].groupindex = 0;
  stats[17].group = 8; stats[17].groupindex = 1; stats[18].group = 8; stats[18].groupindex = 2;

  memset(skillNames, 0, sizeof skillNames);
  stats[97].descval = 1;
  stats[107].descval = 1;
  D2Excel skillTable(TempFile(data->getFile("data\\global\\excel\\skills.txt")));
  for (int i = 0; i < skillTable.rows(); i++)
  {
    int id = atoi(skillTable.value(i, 1));
    char const* name = data->getLocalizedString(String::format("skillname%d", id));
    if (!name) name = data->getLocalizedString(String::format("skillsname%d", id));
    if (!name) name = data->getLocalizedString(String::format("Skillname%d", id + 1));
    if (!name)
    {
      if (*skillTable.value(i, 3))
        int asdf = 0;
      continue;
    }
    skills.add(name, id);
    skillNames[id] = name;

    int cls = data->getCharClass(skillTable.value(i, 2));
    skillClass[id] = cls;

    statMatchers[0].add(String::format("%s %s", str_to, name), MakePair(&stats[97], id));
    if (cls >= 0)
    {
      int mid = getAppendId(data->getLocalizedString(10917 + cls));
      statMatchers[mid].add(String::format("%s %s", str_to, name), MakePair(&stats[107], id));
    }
  }

  for (int i = 0; i < statMatchers.length(); i++)
    statMatchers[i].build();
  statAppendMatch.build();
  skills.build();

  prog_Level = new re::Prog(String::format("%s([0-9]+) (.*) ", str_Level));
  prog_StatLeft = new re::Prog("([-+]?[0-9]+)%? ");
  prog_StatRight = new re::Prog(" ([-+]?[0-9]+)%?");
  prog_StatRange = new re::Prog(" \\+([0-9]+)-([0-9]+)");
  prog_EthSockets = compile(String::format("%s, %s (%%d)",
    data->getLocalizedString(22745), data->getLocalizedString(3453)));

  D2Excel propTable(TempFile(data->getFile("data\\global\\excel\\Properties.txt")));
  for (int i = 0; i < propTable.rows(); i++)
  {
    ItemProp& prop = props.create(propTable.value(i, 0));
    for (int c = 4; c < 32; c += 4)
    {
      int code = atoi(propTable.value(i, c));
      if (!code) break;
      if (code == 36) continue;
      int param = atoi(propTable.value(i, c - 1));
      D2Stat* stat;
      switch (code)
      {
      case 5:
        stat = &stats[21];
        break;
      case 6:
        stat = &stats[22];
        break;
      case 7:
        prop.push(MakePair(code, MakePair(&stats[17], param)));
        stat = &stats[18];
        break;
      case 20:
        stat = &stats[152];
        break;
      case 23:
        stat = &stats[359];
        break;
      default:
        stat = statDir.get(propTable.value(i, c + 1));
      }
      if (stat)
        prop.push(MakePair(code, MakePair(stat, param)));
    }
    if (!strcmp(propTable.value(i, 0), "dmg") && prop.length())
      prop[0].first = 11;
  }
}
void D2StatData::addStatMatch(int mid, char const* desc, D2Stat* stat, int param)
{
  if (strchr(desc, '%'))
  {
    StatReMatch& m = statReMatchers[mid].push();
    m.stat = stat;
    m.param = param;
    m.prog = compile(desc);
  }
  else
  {
    D2Stat* oldStat = statMatchers[mid].geteq(desc).first;
    if (oldStat && (oldStat->descfunc == 1 || oldStat->descfunc == 3 || oldStat->descfunc == 6 || oldStat->descfunc == 9)
      && (stat->descfunc == 2 || stat->descfunc == 4 || stat->descfunc == 7 || stat->descfunc == 8))
    {
      oldStat->percent = stat;
    }
    else if (oldStat && (oldStat->descfunc == 2 || oldStat->descfunc == 4 || oldStat->descfunc == 7 || oldStat->descfunc == 8)
      && (stat->descfunc == 1 || stat->descfunc == 3 || stat->descfunc == 6 || stat->descfunc == 9))
    {
      stat->percent = oldStat;
      statMatchers[mid].add(desc, MakePair(stat, param));
    }
    else
      statMatchers[mid].add(desc, MakePair(stat, param));
  }
} 
static bool finder(re::Match const& match, void* arg)
{
  memcpy(arg, &match, sizeof match);
  return false;
}
bool D2StatData::doParse(char const* str, D2ItemStat& stat)
{
  String statStr(str);
  Pair<int, int> bestEnds;
  int mid = statAppendMatch.get(statStr, false, &bestEnds);
  if (bestEnds.first >= 0)
  {
    statStr.remove(bestEnds.first, bestEnds.second - bestEnds.first);
    statStr.removeTrailingSpaces();
  }

  Pair<D2Stat*, int> bestStat = statMatchers[mid].get(statStr, false, &bestEnds);
  if (!bestStat.first)
    memset(&bestEnds, 0, sizeof bestEnds);
  re::Match bestMatch;
  memset(&bestMatch, 0, sizeof bestMatch);
  bestMatch.start[0] = statStr.c_str() + bestEnds.first;
  bestMatch.end[0] = statStr.c_str() + bestEnds.second;
  for (int i = 0; i < statReMatchers[mid].length(); i++)
  {
    re::Match match;
    if (statReMatchers[mid][i].prog->run(statStr, -1, true, finder, &match) &&
        match.end[0] - match.start[0] > bestMatch.end[0] - bestMatch.start[0])
    {
      bestStat.first = statReMatchers[mid][i].stat;
      bestStat.second = statReMatchers[mid][i].param;
      bestMatch = match;
    }
  }

  if (!bestStat.first || !bestMatch.start[0])
    return false;

  stat.stat = bestStat.first;
  stat.param = bestStat.second;
  stat.value1 = 1;
  stat.value2 = 0;
  switch (stat.stat->descfunc)
  {
  case 11: // format string, one %d
  case 14:
  case 19:
    if (bestMatch.start[1])
      stat.value1 = atoi(bestMatch.start[1]);
    break;
  case 33: // fake, $d-$d range
    if (bestMatch.start[1])
      stat.value1 = atoi(bestMatch.start[1]);
    if (bestMatch.start[2])
      stat.value2 = atoi(bestMatch.start[2]);
    break;
  case 34: // fake, $d-$d range over %d
    if (bestMatch.start[1])
      stat.value1 = atoi(bestMatch.start[1]);
    if (bestMatch.start[3])
    {
      stat.value2 = atoi(bestMatch.start[2]);
      stat.param = atoi(bestMatch.start[3]) * 25;
    }
    else
    {
      stat.value2 = stat.value1;
      stat.param = atoi(bestMatch.start[2]) * 25;
    }
    break;
  case 15: // %d%% chance to cast level %d %s on ...
    if (bestMatch.start[1])
      stat.value1 = atoi(bestMatch.start[1]);
    if (bestMatch.start[2])
      stat.value2 = atoi(bestMatch.start[2]);
    if (bestMatch.start[3])
      stat.param = skills.get(String(bestMatch.start[3], bestMatch.end[3] - bestMatch.start[3]));
    break;
  case 16: // Level %d %s aura
    if (bestMatch.start[1])
      stat.value1 = atoi(bestMatch.start[1]);
    if (bestMatch.start[2])
      stat.param = skills.get(String(bestMatch.start[2], bestMatch.end[2] - bestMatch.start[2]));
    break;
  case 24: // Level $1 $0 (%d/%d Charges)
    {
      if (bestMatch.start[2])
        stat.value1 = atoi(bestMatch.start[2]);
      String sub(statStr.c_str(), bestMatch.start[0] - statStr.c_str());
      re::Match dpos;
      if (prog_Level->run(sub, -1, true, finder, &dpos))
      {
        stat.value2 = atoi(dpos.start[1]);
        stat.param = skills.get(String(dpos.start[2], dpos.end[2] - dpos.start[2]));
        bestMatch.start[0] = statStr.c_str() + (dpos.start[0] - sub.c_str());
      }
      else
        return false;
    }
    break;
  case 23:
    if (String(bestMatch.end[0]) != String::format(" %s", str_Returned))
      return false;
    bestMatch.end[0] = statStr.c_str() + statStr.length();
    stat.param = 1;
    // fall through
  default:
    if (stat.stat->descval)
    {
      if (stat.stat->percent && statStr.indexOf('%') >= 0)
        stat.stat = stat.stat->percent;
      String sub;
      if (stat.stat->descval == 1)
        sub = statStr.substring(0, bestMatch.start[0] - statStr.c_str());
      else
        sub = bestMatch.end[0];
      re::Match dpos;
      if (stat.stat->id == 111 && prog_StatRange->run(sub, -1, true, finder, &dpos))
      {
        stat.value1 = atoi(dpos.start[1]);
        stat.value2 = atoi(dpos.start[2]);
        bestMatch.end[0] += dpos.end[0] - sub.c_str();
      }
      else if ((stat.stat->descval == 1 ? prog_StatLeft : prog_StatRight)->run(sub, -1, true, finder, &dpos))
      {
        stat.value1 = atoi(dpos.start[1]);
        if (stat.stat->descfunc == 20 && stat.value1 < 0)
          stat.value1 = -stat.value1;
        if (stat.stat->descval == 1)
          bestMatch.start[0] = statStr.c_str() + (dpos.start[0] - sub.c_str());
        else
          bestMatch.end[0] += dpos.end[0] - sub.c_str();
        if (stat.stat->id == 111)
          stat.value2 = stat.value1;
      }
      else if (stat.stat->descfunc != 12)
        return false;
    }
  }

  if (bestMatch.start[0] != statStr.c_str())
    return false;
  if (*bestMatch.end[0])
    return false;

  if (stat.stat->id == 126)
    stat.param = 1;
  if (stat.stat->descfunc >= 6 && stat.stat->descfunc <= 9)
    stat.value1 = 0; // can't parse per level stats

  return true;
}
void D2StatData::parse(Array<D2ItemStat>& list, char const* str)
{
  D2ItemStat stat;
  re::Match match;
  if (doParse(str, stat))
    list.push(stat);
  else if (prog_EthSockets->run(str, -1, true, finder, &match))
  {
    stat.stat = &stats[359];
    stat.param = 0; stat.value1 = 1; stat.value2 = 0;
    list.push(stat);
    stat.stat = &stats[194];
    stat.param = 0; stat.value1 = atoi(match.start[1]); stat.value2 = 0;
    list.push(stat);
  }
}

static String orderString(int order, char const* desc, char const* val)
{
  if (order == 1) return String::format("%s %s", val, desc);
  else return String::format("%s %s", desc, val);
}
String D2StatData::format(D2ItemStat const& stat, bool abbrev)
{
  if (!stat.stat)
    return String::null;
  if (abbrev)
  {
    static char const* ab_cls[] = {"ama", "sorc", "nec", "pala", "barb", "druid", "assa"};
    static char const* ab_tab[] = {"bow sk", "p&m", "java sk",
                                   "fire sk", "light sk", "cold sk",
                                   "curses", "p&b", "nec summ",
                                   "pc sk", "off auras", "def auras",
                                   "barb combat", "masteries", "warcries",
                                   "dru summ", "shape sk", "ele sk",
                                   "traps", "shadow sk", "martial arts"};
    char const* src = stat.stat->acronym;
    if (stat.stat->id == 111 && stat.value1 == stat.value2)
      src = "$1 dmg";
    if (!src) return String::null;
    String dst;
    for (int i = 0; src[i]; i++)
    {
      if (src[i] == '$')
      {
        i++;
        if (src[i] == '0')
        {
          switch (stat.stat->id)
          {
          case 83:
            dst += ab_cls[stat.param];
            break;
          case 97:
          case 107:
          case 195:
          case 196:
          case 197:
          case 198:
          case 199:
          case 201:
          case 204:
            dst += String(skillNames[stat.param]).toLower();
            break;
          case 126:
            dst += "fire sk";
            break;
          case 188:
            dst += ab_tab[stat.param];
            break;
          }
        }
        else if (src[i] == '1')
          dst.printf("%d", stat.value1);
        else if (src[i] == '2')
          dst.printf("%d", stat.value2);
      }
      else
        dst += src[i];
    }
    return dst;
  }
  else
  {
    char const* desc = stat.stat->descpos;
    if (stat.value1 < 0) desc = stat.stat->descneg;
    switch (stat.stat->descfunc)
    {
    case 11: // format string, one %d
    case 19:
      return String::format(desc, stat.value1);
    case 14:
      return String::format(data->getLocalizedString(tabString[stat.param]), stat.value1);
    case 33: // fake, $d-$d range
      return String::format(desc, stat.value1, stat.value2);
    case 34: // fake, $d range over %d
      return String::format(desc, stat.value1, stat.param / 25);
    case 15: // %d%% chance to cast level %d %s on ...
      return String::format(desc, stat.value1, stat.value2, skillNames[stat.param]);
    case 16: // Level %d %s aura
      return String::format(desc, stat.value1, skillNames[stat.param]);
    case 24: // Level $1 $0 (%d/%d Charges)
      return String::format("%s%d %s %s", str_Level, stat.value2, skillNames[stat.param],
        String::format(desc, stat.value1, stat.value1));
    case 23:
      return String::format("%d%% %s %s", stat.value1, desc, str_Returned);
    case 27:
      return String::format("+%d %s %s %s", stat.value1, str_to, skillNames[stat.param],
        data->getLocalizedString(10917 + skillClass[stat.param]));
    case 28:
      return String::format("+%d %s %s", stat.value1, str_to, skillNames[stat.param]);
    case 12:
      return (stat.value1 != 1 ? orderString(stat.stat->descval, desc,
        String::format("+%d", stat.value1)) : desc);
    case 13:
      return orderString(stat.stat->descval, data->getLocalizedString(clsString[stat.param]),
        String::format("+%d", stat.value1));
    case 20:
      return orderString(stat.stat->descval, desc, String::format("-%d%%", stat.value1));

    case 1:
      return orderString(stat.stat->descval, desc, String::format("+%d", stat.value1));
    case 2:
      return orderString(stat.stat->descval, desc, String::format("%d%%", stat.value1));
    case 3:
      return orderString(stat.stat->descval, desc, String::format("%d", stat.value1));
    case 4:
      return orderString(stat.stat->descval, desc, String::format("+%d%%", stat.value1));
    case 6:
      return String::format("%s %s", orderString(stat.stat->descval, desc,
        String::format("+%d", stat.value1)), stat.stat->desc2);
    case 7:
      return String::format("%s %s", orderString(stat.stat->descval, desc,
        String::format("%d%%", stat.value1)), stat.stat->desc2);
    case 8:
      return String::format("%s %s", orderString(stat.stat->descval, desc,
        String::format("+%d%%", stat.value1)), stat.stat->desc2);
    case 9:
      return String::format("%s %s", orderString(stat.stat->descval, desc,
        String::format("%d", stat.value1)), stat.stat->desc2);
    }
    return "???";
  }
}

static int findStat(Array<D2ItemStat>& stats, D2Stat* stat, int param = 0, bool add = false)
{
  for (int i = 0; i < stats.length(); i++)
    if (stats[i].stat == stat && (param < 0 || stats[i].param == param ||
        stat->descfunc == 34 || stat->descfunc == 33 || (stat->descfunc >= 6 && stat->descfunc <= 9)))
      return i;
  if (!add) return -1;
  D2ItemStat& res = stats.push();
  res.stat = stat;
  res.param = param;
  res.value1 = res.value2 = 0;
  return stats.length() - 1;
}
static void addStat(Array<D2ItemStat>& stats, D2ItemStat const& stat)
{
  int dst = findStat(stats, stat.stat, stat.param, true);
  stats[dst].value1 += stat.value1;
  if (stat.stat->id == 111)
    stats[dst].value2 += stat.value2;
  else if (stats[dst].value2 < stat.value2)
    stats[dst].value2 = stat.value2;
}
static void subStat(Array<D2ItemStat>& stats, D2ItemStat const& stat)
{
  int dst = findStat(stats, stat.stat, stat.param);
  if (dst < 0) return;
  stats[dst].value1 -= stat.value1;
  if (stat.stat->id == 111)
    stats[dst].value2 -= stat.value2;
  if (stats[dst].value1 <= 0 && (stat.stat->id != 111 || stats[dst].value2 <= 0))
    stats.remove(dst, 1);
}
void D2StatData::ungroup(Array<D2ItemStat>& list)
{
  Array<D2ItemStat> dest;
  for (int i = 0; i < list.length(); i++)
  {
    if (list[i].stat->id >= GROUP_BASE)
    {
      for (int j = 0; j < 512; j++)
      {
        if (stats[j].group != list[i].stat->id - GROUP_BASE)
          continue;
        D2ItemStat stat;
        stat.stat = &stats[j];
        if (stats[j].groupindex == 0)
          stat.value1 = list[i].param;
        else if (stats[j].groupindex == 1)
          stat.value1 = list[i].value1;
        else if (stats[j].groupindex == 2)
          stat.value1 = list[i].value2;
        addStat(dest, stat);
      }
    }
    else
      addStat(dest, list[i]);
  }
  list = dest;
}
void D2StatData::group(Array<D2ItemStat>& list)
{
  Array<D2ItemStat> dest;
  int groups[32]; int ngroups = 0;
  for (int i = 0; i < list.length(); i++)
  {
    if (list[i].stat->group >= 0)
    {
      int j;
      for (j = 0; j < ngroups; j++)
        if (groups[j] == list[i].stat->group)
          break;
      if (j == ngroups)
        groups[ngroups++] = list[i].stat->group;
    }
    else
      addStat(dest, list[i]);
  }
  for (int i = 0; i < ngroups; i++)
  {
    D2ItemStat stat;
    stat.stat = &stats[groups[i] + GROUP_BASE];
    bool first = true;
    for (int j = 0; j < list.length(); j++)
    {
      if (list[j].stat->group != groups[i])
        continue;
      if (list[j].stat->groupindex == 0)
        stat.param = list[j].value1;
      else if (list[j].stat->groupindex == 1)
      {
        if (first)
        {
          stat.value1 = list[j].value1;
          first = false;
        }
        else if (list[j].value1 < stat.value1)
          stat.value1 = list[j].value1;
      }
      else if (list[j].stat->groupindex == 2)
        stat.value2 = list[j].value1;
    }
    addStat(dest, stat);
    for (int j = 0; j < list.length(); j++)
    {
      if (list[j].stat->group != groups[i])
        continue;
      if (list[j].stat->groupindex == 0)
        list[j].value1 -= stat.param;
      else if (list[j].stat->groupindex == 1)
        list[j].value1 -= stat.value1;
      else if (list[j].stat->groupindex == 2)
        list[j].value1 -= stat.value2;
      if (list[j].value1)
        addStat(dest, list[j]);
    }
  }
  list = dest;
}

static int statCompare(D2ItemStat const& a, D2ItemStat const& b)
{
  if (a.stat->sort != b.stat->sort)
    return b.stat->sort - a.stat->sort;
  if (a.param != b.param)
    return b.param - a.param;
  return int(b.stat) - int(a.stat);
}
String D2StatData::processTorch(Array<D2ItemStat>& list)
{
  int cls = findStat(list, &stats[83], -1);
  int att = findStat(list, &stats[361]);
  int res = findStat(list, &stats[362]);
  if (cls < 0 || att < 0 || res < 0)
    return "";
  return String::format("%s %d/%d", String(data->getClassName(list[cls].param)).substring(0, 4),
    list[att].value1, list[res].value1);
}
String D2StatData::processAnni(Array<D2ItemStat>& list)
{
  int att = findStat(list, &stats[361]);
  int res = findStat(list, &stats[362]);
  int exp = findStat(list, &stats[85]);
  if (att < 0 || res < 0 || exp < 0)
    return "";
  return String::format("%d/%d/%d", list[att].value1, list[res].value1, list[exp].value1);
}
String D2StatData::processRBF(Array<D2ItemStat>& list)
{
  int stat1 = -1, stat2 = -1;
  String text;
  if (findStat(list, &stats[197], 59) >= 0)
    text = "cold die ", stat1 = 335, stat2 = 331;
  else if (findStat(list, &stats[197], 56) >= 0)
    text = "fire die ", stat1 = 333, stat2 = 329;
  else if (findStat(list, &stats[197], 53) >= 0)
    text = "light die ", stat1 = 334, stat2 = 330;
  else if (findStat(list, &stats[197], 92) >= 0)
    text = "psn die ", stat1 = 336, stat2 = 332;
  else if (findStat(list, &stats[199], 44) >= 0)
    text = "cold up ", stat1 = 335, stat2 = 331;
  else if (findStat(list, &stats[199], 46) >= 0)
    text = "fire up ", stat1 = 333, stat2 = 329;
  else if (findStat(list, &stats[199], 48) >= 0)
    text = "light up ", stat1 = 334, stat2 = 330;
  else if (findStat(list, &stats[199], 278) >= 0)
    text = "psn up ", stat1 = 336, stat2 = 332;
  if (stat1 >= 0) stat1 = findStat(list, &stats[stat1]);
  if (stat2 >= 0) stat2 = findStat(list, &stats[stat2]);
  if (stat1 < 0 || stat2 < 0)
    return "";
  text.printf(" -%d/+%d", list[stat1].value1, list[stat2].value2);
  return text;
}
String D2StatData::process(D2Item* item)
{
  if (item->base->type->isType("jewl") && item->quality == D2Item::qUnique)
    return processRBF(item->stats);
  if (item->base->type->isType("mcha") && item->quality == D2Item::qUnique)
    return processTorch(item->stats);
  if (item->base->type->isType("scha") && item->quality == D2Item::qUnique)
    return processAnni(item->stats);

  Array<D2ItemStat> list = item->stats;

  bool hasDef = false;
  if (item->unique && item->unique->base == item->base && findStat(list, &stats[16]) >= 0)
    hasDef = true;
  if (!item->unique && findStat(list, &stats[16]) >= 0)
    hasDef = true;
  if (item->unique && item->unique->base == item->base && (item->flags & D2Item::fUnidentified))
  {
    if (findStat(item->unique->preset, &stats[16]) >= 0)
      hasDef = true;
    else
    {
      int ppdef = findStat(item->unique->preset, &stats[31]);
      int def = findStat(list, &stats[360]);
      if (ppdef >= 0 && def >= 0)
        list[def].value1 += item->unique->preset[ppdef].value1;
    }
  }
  if (hasDef)
  {
    int def = findStat(list, &stats[360]);
    if (def >= 0) list.remove(def, 1);
  }
  if (item->unique && item->unique->type == 6)
  {
    int sox = findStat(list, &stats[194]);
    if (sox >= 0) list.remove(sox, 1);
  }
  if (item->unique || item->socketItems.length())
  {
    ungroup(list);

    for (int i = 0; i < item->socketItems.length(); i++)
    {
      D2Item* sub = item->socketItems[i];
      if (!sub) continue;
      Array<D2ItemStat> subStats;
      if (sub->base && sub->base->numGemMods == 1)
        subStats = sub->base->gemMods[0];
      else if (sub->base && sub->base->numGemMods == 3)
      {
        if (item->base->type->isType("weap"))
          subStats = sub->base->gemMods[0];
        else if (item->base->type->isType("shld"))
          subStats = sub->base->gemMods[2];
        else if (item->base->type->isType("tors") || item->base->type->isType("helm"))
          subStats = sub->base->gemMods[1];
      }
      else
        subStats = sub->stats;
      ungroup(subStats);
      for (int j = 0; j < subStats.length(); j++)
        subStat(list, subStats[j]);
    }

    if (item->unique)
    {
      for (int i = 0; i < item->unique->preset.length(); i++)
      {
        D2ItemStat stat = item->unique->preset[i];
        int pos = findStat(list, stat.stat, stat.param);
        if (pos >= 0 && list[pos].value1 <= stat.value1 && list[pos].value2 <= stat.value2)
          list.remove(pos, 1);
      }
    }

    group(list);
  }

  list.sort(statCompare);
  String text;
  bool comma = false;
  for (int i = 0; i < list.length(); i++)
  {
    String fmt = format(list[i], true);
    if (fmt.length())
    {
      if (text.length() && s_isalpha(fmt[0]))
        text += ',', comma = true;
      else if (comma)
        text += ',', comma = false;
      if (text.length())
        text += ' ';
      text += fmt;
    }
  }
  return text;
}
void D2StatData::addPreset(Array<D2ItemStat>& list, char const* propname, int par, int val1, int val2)
{
  if (!props.has(propname))
    return;
  if (!strcmp(propname, "dmg-pois"))
  {
    val1 = (val1 * par + 128) / 256;
    val2 = (val2 * par + 128) / 256;
  }
  ItemProp& prop = props.get(propname);
  for (int i = 0; i < prop.length(); i++)
  {
    D2ItemStat stat;
    stat.stat = prop[i].second.first;
    if (prop[i].first == 21) stat.param = prop[i].second.second;
    stat.value1 = stat.value2 = 0;
    switch (prop[i].first)
    {
    case 12:
    case 36:
      break;
    case 15:
      stat.value1 = val1;
      break;
    case 16:
      stat.value1 = val2;
      break;
    case 17:
      stat.value1 = par;
      break;
    case 11:
    case 19:
      stat.value1 = val1;
      stat.value2 = val2;
      stat.param = par;
      break;
    case 10:
    case 22:
      stat.param = par;
      // fall through
    default:
      if (val1 == val2)
        stat.value1 = val1;
    }
    if (stat.value1 || stat.value2)
      addStat(list, stat);
  }
}
