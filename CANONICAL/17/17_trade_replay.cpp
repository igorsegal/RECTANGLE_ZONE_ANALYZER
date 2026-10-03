#include "../02/xfbar_reader.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace fs = std::filesystem;
using rza::canonical::Bar;
using rza::canonical::XfbarData;
using rza::canonical::read_xfbar;

namespace {
constexpr int TP_POINTS = 89;
constexpr int SL_POINTS = 233;
constexpr double START_DEPOSIT_USD = 10000.0;
constexpr int LEVERAGE = 500;
constexpr double MIN_MARGIN_LEVEL_PCT = 500.0;
const std::array<std::string,10> BASKET{{
 "DJ30","NAS100","GER40","SP500","US2000",
 "XAUUSD","XAUAUD","XAUJPY","XPDUSD","XPTUSD"}};

struct Event { std::string symbol, dir; std::int64_t signal=0, departure=0; double zl=0, zh=0; };
enum class Outcome { TP, SL, AMBIG_SL, CENSORED, INVALID };
const char* oname(Outcome x){ switch(x){case Outcome::TP:return "TP";case Outcome::SL:return "SL";case Outcome::AMBIG_SL:return "SL_AMBIGUOUS";case Outcome::CENSORED:return "CENSORED";default:return "INVALID";} }
struct Trade {
 Event e; Outcome out=Outcome::INVALID; std::int64_t entry_t=0, exit_t=0; double point=0, entry=0,tp=0,sl=0,exit=0,risk_pts=0,pnl_pts=0,R=0; int entry_spread=0,exit_spread=0;
};
struct Stats {
 std::uint64_t n=0,closed=0,wins=0,losses=0,ambig=0,censored=0,invalid=0; long double sumR=0,gpR=0,glR=0,sumRisk=0;
 double minR=std::numeric_limits<double>::infinity(), maxR=-std::numeric_limits<double>::infinity();
 void add(const Trade&t){++n;if(t.out==Outcome::CENSORED){++censored;return;}if(t.out==Outcome::INVALID){++invalid;return;}++closed;sumR+=t.R;sumRisk+=t.risk_pts;minR=std::min(minR,t.R);maxR=std::max(maxR,t.R);if(t.out==Outcome::TP){++wins;gpR+=std::max(0.0,t.R);}else{++losses;if(t.out==Outcome::AMBIG_SL)++ambig;glR+=std::max(0.0,-t.R);}}
 double wr()const{return closed?100.0*double(wins)/double(closed):std::numeric_limits<double>::quiet_NaN();}
 double avgR()const{return closed?double(sumR/closed):std::numeric_limits<double>::quiet_NaN();}
 double pf()const{return glR>0?double(gpR/glR):(gpR>0?std::numeric_limits<double>::infinity():std::numeric_limits<double>::quiet_NaN());}
 double avgRisk()const{return closed?double(sumRisk/closed):std::numeric_limits<double>::quiet_NaN();}
};

bool selected(const std::string&s){return std::find(BASKET.begin(),BASKET.end(),s)!=BASKET.end();}
std::vector<std::string> split(const std::string&s){std::vector<std::string>v;std::stringstream ss(s);std::string x;while(std::getline(ss,x,';'))v.push_back(x);return v;}
bool i64(const std::string&s,std::int64_t&v){try{std::size_t p=0;long long x=std::stoll(s,&p);if(p!=s.size())return false;v=static_cast<std::int64_t>(x);return true;}catch(...){return false;}}
bool dbl(const std::string&s,double&v){try{std::size_t p=0;v=std::stod(s,&p);return p==s.size()&&std::isfinite(v);}catch(...){return false;}}

bool load_events(const fs::path&p,std::vector<Event>&out,std::string&err){
 std::ifstream f(p,std::ios::binary);if(!f){err="cannot_open_events";return false;}std::string line;if(!std::getline(f,line)){err="empty_events";return false;}if(!line.empty()&&line.back()=='\r')line.pop_back();auto h=split(line);std::map<std::string,std::size_t>c;for(std::size_t i=0;i<h.size();++i)c[h[i]]=i;for(const char*n:{"Symbol","Direction","SignalTime","ZoneLow","ZoneHigh","DepartureTime"})if(!c.count(n)){err=std::string("missing_column=")+n;return false;}
 std::uint64_t row=1;while(std::getline(f,line)){++row;if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty())continue;auto x=split(line);if(x.size()<h.size()){err="bad_csv_row="+std::to_string(row);return false;}if(!selected(x[c["Symbol"]]))continue;Event e;e.symbol=x[c["Symbol"]];e.dir=x[c["Direction"]];if((e.dir!="BULLISH"&&e.dir!="BEARISH")||!i64(x[c["SignalTime"]],e.signal)||!dbl(x[c["ZoneLow"]],e.zl)||!dbl(x[c["ZoneHigh"]],e.zh)||!i64(x[c["DepartureTime"]],e.departure)||!(e.zl<e.zh)||e.departure<=0){err="bad_event_row="+std::to_string(row);return false;}out.push_back(e);}
 std::sort(out.begin(),out.end(),[](const Event&a,const Event&b){return std::tie(a.departure,a.symbol,a.signal)<std::tie(b.departure,b.symbol,b.signal);});return true;
}

std::size_t find_time(const std::vector<Bar>&b,std::int64_t t){auto it=std::lower_bound(b.begin(),b.end(),t,[](const Bar&x,std::int64_t y){return x.time<y;});return(it!=b.end()&&it->time==t)?std::size_t(it-b.begin()):b.size();}
Trade replay(const Event&e,const XfbarData&m){
 Trade t;t.e=e;t.point=m.point;auto d=find_time(m.bars,e.departure);if(d==m.bars.size()||d+1>=m.bars.size()||!(m.point>0)){return t;}std::size_t ei=d+1;const Bar&eb=m.bars[ei];bool buy=e.dir=="BULLISH";t.entry_t=eb.time;t.entry_spread=std::max(0,eb.spread);t.entry=buy?eb.open+t.entry_spread*m.point:eb.open;t.tp=buy?t.entry+TP_POINTS*m.point:t.entry-TP_POINTS*m.point;t.sl=buy?t.entry-SL_POINTS*m.point:t.entry+SL_POINTS*m.point;double risk=buy?t.entry-t.sl:t.sl-t.entry;if(!(risk>0))return t;t.risk_pts=risk/m.point;
 for(std::size_t i=ei;i<m.bars.size();++i){const Bar&b=m.bars[i];int sp=std::max(0,b.spread);double s=sp*m.point;Outcome o=Outcome::INVALID;double xp=0;if(buy){if(b.open<=t.sl){o=Outcome::SL;xp=b.open;}else if(b.open>=t.tp){o=Outcome::TP;xp=b.open;}else{bool hs=b.low<=t.sl,ht=b.high>=t.tp;if(hs&&ht){o=Outcome::AMBIG_SL;xp=t.sl;}else if(hs){o=Outcome::SL;xp=t.sl;}else if(ht){o=Outcome::TP;xp=t.tp;}}}else{double ao=b.open+s,ah=b.high+s,al=b.low+s;if(ao>=t.sl){o=Outcome::SL;xp=ao;}else if(ao<=t.tp){o=Outcome::TP;xp=ao;}else{bool hs=ah>=t.sl,ht=al<=t.tp;if(hs&&ht){o=Outcome::AMBIG_SL;xp=t.sl;}else if(hs){o=Outcome::SL;xp=t.sl;}else if(ht){o=Outcome::TP;xp=t.tp;}}}if(o!=Outcome::INVALID){t.out=o;t.exit_t=b.time;t.exit_spread=sp;t.exit=xp;double pnl=buy?t.exit-t.entry:t.entry-t.exit;t.pnl_pts=pnl/m.point;t.R=pnl/risk;return t;}}
 t.out=Outcome::CENSORED;t.exit_t=m.bars.back().time;return t;
}

int year_utc(std::int64_t ts){
 std::time_t tt=static_cast<std::time_t>(ts);
 const std::tm* tm=std::gmtime(&tt);
 return tm ? tm->tm_year+1900 : 0;
}
void header(std::ostream&o,const char*k){o<<k<<";Eligible;Closed;Wins;Losses;WinRatePct;AmbiguousAsSL;Censored;Invalid;AvgR;ProfitFactorR;SumR;AvgRiskPoints;MinR;MaxR\n";}
void row(std::ostream&o,const std::string&k,const Stats&s){o<<k<<';'<<s.n<<';'<<s.closed<<';'<<s.wins<<';'<<s.losses<<';'<<std::setprecision(12)<<s.wr()<<';'<<s.ambig<<';'<<s.censored<<';'<<s.invalid<<';'<<s.avgR()<<';'<<s.pf()<<';'<<double(s.sumR)<<';'<<s.avgRisk()<<';';if(s.closed)o<<s.minR<<';'<<s.maxR;else o<<"NA;NA";o<<'\n';}
std::uint64_t max_concurrent(const std::vector<Trade>&v){struct M{std::int64_t t;int d,o;};std::vector<M>a;for(const auto&x:v)if(x.out!=Outcome::INVALID&&x.entry_t){a.push_back({x.entry_t,1,0});a.push_back({x.exit_t,-1,1});}std::sort(a.begin(),a.end(),[](const M&x,const M&y){return std::tie(x.t,x.o)<std::tie(y.t,y.o);});std::int64_t cur=0;std::uint64_t mx=0;for(auto&m:a){cur+=m.d;if(cur<0)cur=0;mx=std::max(mx,std::uint64_t(cur));}return mx;}

int selftest(){int f=0;auto ck=[&](bool x,const char*m){std::cout<<(x?"[OK]   ":"[FAIL] ")<<m<<'\n';if(!x)++f;};XfbarData m;m.success=true;m.symbol="XAUUSD";m.period_seconds=300;m.point=.01;m.bars={{1000,100,100.2,99.9,100.1,2},{1300,100.3,101.5,100.2,101.2,2}};Event e{"XAUUSD","BULLISH",1000,1000,99,100};Trade t=replay(e,m);ck(t.out==Outcome::TP,"causal BUY reaches TP89");ck(std::abs(t.entry-100.32)<1e-9,"BUY entry uses Ask spread");ck(std::abs(t.sl-97.99)<1e-9,"BUY SL is fixed 233 points from actual entry");XfbarData a=m;a.bars={{1000,100,100.1,99.9,100,0},{1300,100.3,101.5,97,100,0}};Trade q=replay(e,a);ck(q.out==Outcome::AMBIG_SL,"same-bar TP+SL is conservative SL");return f?1:0;}
} // namespace

int main(int argc,char**argv){
 if(argc>=2&&std::string(argv[1])=="--selftest")return selftest();
 fs::path events=argc>=2?argv[1]:"../15out/15_EVENTS.csv",raw=argc>=3?argv[2]:"D:/AHexaTrader/1DataFiles/raw",out=argc>=4?argv[3]:"../16out";std::error_code ec;fs::create_directories(out,ec);if(ec){std::cerr<<"BLOCK17 FAIL - CANNOT_CREATE_OUTDIR\n";return 2;}
 std::vector<Event>ev;std::string err;if(!load_events(events,ev,err)){std::cerr<<"BLOCK17 FAIL - "<<err<<'\n';return 3;}std::map<std::string,XfbarData>m5;for(const auto&s:BASKET){fs::path p=raw/s/(s+"_M5.bin");XfbarData d=read_xfbar(p.string());if(!d.success||d.symbol!=s||d.period_seconds!=300){std::cerr<<"BLOCK17 FAIL - M5 "<<s<<" file="<<p.string()<<" reason="<<d.error<<'\n';return 4;}m5.emplace(s,std::move(d));}
 std::ofstream tr(out/"17_TRADES.csv",std::ios::binary),g(out/"17_GLOBAL.csv",std::ios::binary),bs(out/"17_BY_SYMBOL.csv",std::ios::binary),by(out/"17_BY_YEAR.csv",std::ios::binary);if(!tr||!g||!bs||!by){std::cerr<<"BLOCK17 FAIL - OUTPUT_OPEN\n";return 5;}tr<<"Symbol;Direction;SignalTime;DepartureTime;EntryTime;ZoneLow;ZoneHigh;Point;EntryPrice;TPPrice;SLPrice;ExitTime;ExitPrice;Outcome;EntrySpreadPoints;ExitSpreadPoints;RiskPoints;PnLPoints;R\n";
 Stats all;std::map<std::string,Stats>ss;std::map<int,Stats>yy;std::vector<Trade>tv;tv.reserve(ev.size());std::uint64_t n=0;for(const auto&e:ev){Trade t=replay(e,m5.at(e.symbol));all.add(t);ss[e.symbol].add(t);yy[year_utc(t.entry_t?t.entry_t:e.departure)].add(t);tv.push_back(t);tr<<e.symbol<<';'<<e.dir<<';'<<e.signal<<';'<<e.departure<<';'<<t.entry_t<<';'<<std::setprecision(12)<<e.zl<<';'<<e.zh<<';'<<t.point<<';'<<t.entry<<';'<<t.tp<<';'<<t.sl<<';'<<t.exit_t<<';'<<t.exit<<';'<<oname(t.out)<<';'<<t.entry_spread<<';'<<t.exit_spread<<';'<<t.risk_pts<<';'<<t.pnl_pts<<';'<<t.R<<'\n';if(++n%5000==0)std::cout<<"PROGRESS "<<n<<'/'<<ev.size()<<" closed="<<all.closed<<" wins="<<all.wins<<" losses="<<all.losses<<'\n';}
 header(g,"Scope");row(g,"CORE10",all);header(bs,"Symbol");for(const auto&s:BASKET)row(bs,s,ss[s]);header(by,"Year");for(const auto&x:yy)row(by,std::to_string(x.first),x.second);auto mx=max_concurrent(tv);std::ofstream sm(out/"17_SUMMARY.txt",std::ios::binary);sm<<"RZA BLOCK 17 - CORE10 FIXED FIBO RISK REPLAY\nBASKET=DJ30,NAS100,GER40,SP500,US2000,XAUUSD,XAUAUD,XAUJPY,XPDUSD,XPTUSD\nSOURCE_EVENTS=BLOCK15_FIX3_89_ONLY\nENTRY=NEXT_M5_OPEN_AFTER_CONFIRMED_DEPARTURE_CLOSE\nBUY_ENTRY=ASK_OPEN_USING_XFBAR_SPREAD\nSELL_ENTRY=BID_OPEN\nTP_POINTS_FROM_ACTUAL_ENTRY=89\nSL_BUY=233_POINTS_BELOW_ACTUAL_ENTRY\nSL_SELL=233_POINTS_ABOVE_ACTUAL_ENTRY\nSELL_TP_SL_TRIGGER=ASK_APPROX_BID_PLUS_BAR_SPREAD\nSAME_M5_TP_AND_SL=CONSERVATIVE_SL\nSLIPPAGE_POINTS=0_NOT_INVENTED\nCOMMISSION=NOT_INCLUDED_NO_BROKER_SPEC\nSTART_DEPOSIT_USD="<<START_DEPOSIT_USD<<"\nACCOUNT_LEVERAGE=1:"<<LEVERAGE<<"\nMIN_MARGIN_LEVEL_PCT="<<MIN_MARGIN_LEVEL_PCT<<"\nLOT_POLICY=BROKER_MINIMUM_LOT\nACCOUNT_USD_PNL_AND_MARGIN=DEFERRED_UNTIL_BROKER_SYMBOL_SPECS\nELIGIBLE_TRADES="<<all.n<<"\nCLOSED_TRADES="<<all.closed<<"\nWINS="<<all.wins<<"\nLOSSES="<<all.losses<<"\nAMBIGUOUS_AS_SL="<<all.ambig<<"\nCENSORED="<<all.censored<<"\nINVALID="<<all.invalid<<"\n"<<std::setprecision(12)<<"WIN_RATE_PCT="<<all.wr()<<"\nAVG_R="<<all.avgR()<<"\nPROFIT_FACTOR_R="<<all.pf()<<"\nSUM_R="<<double(all.sumR)<<"\nAVG_INITIAL_RISK_POINTS="<<all.avgRisk()<<"\nMAX_CONCURRENT_TRADES="<<mx<<'\n';
 std::cout<<"============================================================\nRZA BLOCK 17 PASS\nCORE10 CLOSED="<<all.closed<<" WINS="<<all.wins<<" LOSSES="<<all.losses<<"\n"<<std::setprecision(8)<<"WIN_RATE="<<all.wr()<<"% AVG_R="<<all.avgR()<<" PF_R="<<all.pf()<<"\nMAX_CONCURRENT="<<mx<<"\n============================================================\n";return 0;
}
