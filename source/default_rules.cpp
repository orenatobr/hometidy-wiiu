#include "default_rules.h"

// Release years come from memory and only decide the order inside a series.
// Edit sd:/wiiu/apps/HomeTidy/series.txt to override or extend this table.
const char* kDefaultRules = R"RULES(
# pattern | series | year           (first match wins; patterns are normalized)
# --- Zelda
hyrule warriors            | zelda | 2014
ocarina of time            | zelda | 1998
majoras mask               | zelda | 2000
wind waker                 | zelda | 2013   # HD release, as in the order approved on the console
twilight princess          | zelda | 2016   # HD release
skyward sword              | zelda | 2011
breath of the wild         | zelda | 2017
link to the past           | zelda | 1991
links awakening            | zelda | 1993
zelda                      | zelda | 1990
# --- Mario Kart / Party / sports spin-offs
mario kart wii             | mario kart | 2008
mario kart 8               | mario kart | 2014
mario kart                 | mario kart | 1992
mario party                | mario party | 2010
mario strikers             | mario strikers | 2007
mario sonic                | mario sonic | 2007
mario tennis               | mario tennis | 2015
mario golf                 | mario golf | 2014
paper mario                | paper mario | 2007
luigis mansion            | luigis mansion | 2001
captain toad               | captain toad | 2014
yoshis                    | yoshi | 2015
yoshi                      | yoshi | 1991
# --- Mario platformers
super mario sunshine       | super mario | 2002
super mario galaxy more    | super mario | 2010
super mario galaxy 2       | super mario | 2010
super mario galaxy         | super mario | 2007
new super mario bros u     | super mario | 2012
new super mario bros wii   | super mario | 2009
new super mario bros       | super mario | 2006
super mario 3d world       | super mario | 2013
super mario maker          | super mario | 2015
super mario world          | super mario | 1990
super mario bros 3         | super mario | 1988
super mario bros 2         | super mario | 1988
super mario bros           | super mario | 1985
super mario all stars      | super mario | 1993
super mario 64             | super mario | 1996
super mario                | super mario | 1990
# --- Other Nintendo series
donkey kong country tropical | donkey kong | 2014
donkey kong country returns  | donkey kong | 2010
donkey kong country          | donkey kong | 1994
donkey kong                  | donkey kong | 1981
kirbys epic yarn            | kirby | 2010
kirbys return               | kirby | 2011
kirby and the rainbow        | kirby | 2015
kirby                        | kirby | 1992
super metroid                | metroid | 1994
metroid prime 3              | metroid | 2007
metroid prime                | metroid | 2002
metroid other m              | metroid | 2010
metroid                      | metroid | 1986
super smash bros             | smash bros | 2014
smash bros brawl             | smash bros | 2008
pokemon rumble               | pokemon | 2009
pokemon                      | pokemon | 1998
xenoblade chronicles x       | xenoblade | 2015
xenoblade                    | xenoblade | 2010
fire emblem                  | fire emblem | 2007
splatoon                     | splatoon | 2015
wii sports resort            | wii sports | 2009
wii sports club              | wii sports | 2014
wii sports                   | wii sports | 2006
wii fit u                    | wii fit | 2013
wii fit                      | wii fit | 2008
wii party u                  | wii party | 2013
wii party                    | wii party | 2010
wii play motion              | wii play | 2011
wii play                     | wii play | 2006
wii music                    | wii music | 2008
pikmin                       | pikmin | 2001
star fox                     | star fox | 1993
f zero                       | f zero | 1990
animal crossing              | animal crossing | 2008
punch out                    | punch out | 2009
kid icarus                   | kid icarus | 1986
rhythm heaven                | rhythm heaven | 2011
nes remix                    | nes remix | 2013
sin and punishment           | sin and punishment | 2009
the last story               | last story | 2011
pandoras tower              | pandoras tower | 2011
monster hunter               | monster hunter | 2009
earthbound                   | earthbound | 1994
mother 3                     | earthbound | 2006
# --- Third party
bayonetta                    | bayonetta | 2014
rayman                       | rayman | 2013
lego                         | lego | 2013
just dance                   | just dance | 2014
call of duty                 | call of duty | 2012
assassins creed             | assassins creed | 2012
resident evil                | resident evil | 2002
sonic                        | sonic | 1991
final fight                  | final fight | 1991
street fighter               | street fighter | 1991
mega man                     | mega man | 1987
castlevania                  | castlevania | 1986
contra                       | contra | 1987
)RULES";
