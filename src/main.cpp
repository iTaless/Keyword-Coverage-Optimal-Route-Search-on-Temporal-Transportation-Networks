#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <queue>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
using namespace std;
using Clock=chrono::steady_clock;
constexpr double INF=numeric_limits<double>::infinity();
constexpr int TINF=numeric_limits<int>::max();
vector<string> csv(const string& line) {
    vector<string> a;
    string s;
    bool quote=false;
    for(size_t i=0;i<line.size();++i){
        char c=line[i];
        if(c=='"') {
            if(quote && i+1<line.size() && line[i+1]=='"'){
                s+='"';
                ++i;
            }
            else quote=!quote;
        }
        else if(c==','&&!quote){
            a.push_back(s);
            s.clear();
        }
        else s+=c;
    }
    if(quote) throw runtime_error("unterminated CSV quote");
    a.push_back(s);
    return a;
}
vector<string> split(const string& s,char sep){
    vector<string> out;
    string cur;
    for(char c:s){
        if(c==sep){
            out.push_back(cur);
            cur.clear();
        }
        else cur+=c;
    }
    out.push_back(cur);
    return out;
}
struct Table {
    vector<string> names;
    unordered_map<string,size_t> index;
    vector<vector<string>> rows;
    explicit Table(const string& path,bool load_rows=true){
        ifstream f(path);
        if(!f)throw runtime_error("cannot open "+path);
        string line;
        if(!getline(f,line))throw runtime_error("empty CSV "+path);
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        names=csv(line);
        if(!names.empty()&&!names[0].empty()&&(unsigned char)names[0][0]==0xef)names[0].erase(0,3);
        for(size_t i=0;i<names.size();++i) index[names[i]]=i;
        if(!load_rows)return;
        while(getline(f,line)){
            if(!line.empty()&&line.back()=='\r')line.pop_back();
            if(line.empty())continue;
            auto x=csv(line);
            if(x.size()!=names.size())throw runtime_error("CSV column count in "+path);
            rows.push_back(move(x));
        }
    }
    string get(const vector<string>& row,const string& key)const{
        auto it=index.find(key);
        if(it==index.end())throw runtime_error("missing CSV column "+key);
        return row[it->second];
    }
};
struct Vertex{
    string id;
    double dwell=0;
    unordered_map<string,double> cov;
};
struct Edge{
    string id;
    int from,to,dep,arr;
    double cost;
};
struct Graph{
    vector<Vertex> v;
    vector<Edge> e;
    unordered_map<string,int> vertices;
    vector<vector<int>> out;
    int vertex(const string& s){
        auto it=vertices.find(s);
        if(it!=vertices.end())return it->second;
        int n=(int)v.size();
        vertices[s]=n;
        v.push_back({
            s,0,{}
        }
        );
        return n;
    }
    void read(const string& stations,const string& trips){
        Table st(stations),tr(trips,false);
        unordered_set<string> station_rows,ids;
        for(const auto& x:st.rows){
            string id=st.get(x,"station_id");
            if(id.empty())throw runtime_error("empty station_id");
            int i=vertex(id);
            double d=stod(st.get(x,"dwell_min"));
            if(d<0||!isfinite(d))throw runtime_error("invalid dwell");
            if(station_rows.count(id)&&v[i].dwell!=d)throw runtime_error("inconsistent dwell");
            station_rows.insert(id);
            v[i].dwell=d;
            string key=st.get(x,"keyword");
            if(!key.empty()){
                double c=stod(st.get(x,"coverage"));
                if(c<0||c>1||!isfinite(c))throw runtime_error("invalid coverage");
                if(v[i].cov.count(key))throw runtime_error("duplicate station keyword");
                v[i].cov[key]=c;
            }
        }
        ifstream trip_file(trips);
        string trip_line;
        getline(trip_file,trip_line); // already validated by Table
        while(getline(trip_file,trip_line)){
            if(!trip_line.empty()&&trip_line.back()=='\r')trip_line.pop_back();
            if(trip_line.empty())continue;
            auto x=csv(trip_line);
            if(x.size()!=tr.names.size())throw runtime_error("CSV column count in "+trips);
            string id=tr.get(x,"edge_id"),a=tr.get(x,"from"),b=tr.get(x,"to");
            if(!ids.insert(id).second||id.empty())throw runtime_error("duplicate/empty edge ID");
            if(!vertices.count(a)||!vertices.count(b))throw runtime_error("trip references missing station");
            int dep=stoi(tr.get(x,"departure_min")),arr=stoi(tr.get(x,"arrival_min"));
            double cost=stod(tr.get(x,"cost"));
            if(dep<0||arr<dep||cost<0||!isfinite(cost))throw runtime_error("invalid trip time/cost");
            e.push_back({
                id,vertices.at(a),vertices.at(b),dep,arr,cost
            }
            );
        }
        out.assign(v.size(),{}
        );
        for(int i=0;i<(int)e.size();++i)out[e[i].from].push_back(i);
        for(auto& x:out)sort(x.begin(),x.end(),[&](int a,int b){
            return e[a].dep!=e[b].dep?e[a].dep<e[b].dep:e[a].id<e[b].id;
        }
        );
    }
};
struct Query {
    string id,city,parameter,value;
    int source,target,start,end,k;
    double budget;
    vector<string> keys;
    vector<double> weights;
    uint64_t seed;
};
Query read_query(const string& path,const string& id,const Graph& g){
    Table t(path);
    for(const auto& row:t.rows){
        if(t.get(row,"query_id")!=id)continue;
        Query q;
        q.id=id;
        q.city=t.get(row,"city");
        q.parameter=t.get(row,"parameter");
        q.value=t.get(row,"value");
        q.source=g.vertices.at(t.get(row,"source"));
        q.target=g.vertices.at(t.get(row,"target"));
        q.start=stoi(t.get(row,"start_min"));
        q.end=stoi(t.get(row,"end_min"));
        q.budget=stod(t.get(row,"budget"));
        q.k=stoi(t.get(row,"k"));
        q.seed=stoull(t.get(row,"seed"));
        q.keys=split(t.get(row,"keywords"),'|');
        for(const auto& w:split(t.get(row,"weights"),'|'))q.weights.push_back(stod(w));
        if(q.source==q.target||q.start<0||q.end<q.start||q.budget<0||!isfinite(q.budget)||q.k<1||q.keys.empty()||q.keys.size()!=q.weights.size())throw runtime_error("invalid query");
        unordered_set<string> distinct(q.keys.begin(),q.keys.end());
        if(distinct.size()!=q.keys.size())throw runtime_error("duplicate keyword");
        for(double w:q.weights)if(w<=0||!isfinite(w))throw runtime_error("invalid keyword weight");
        return q;
    }
    throw runtime_error("query_id not found: "+id);
}
struct Arc{
    int from,to,edge;
    double cost;
};
// Query-specific event graph: waiting arcs and every parallel scheduled trip.
// Algorithm 1 uses its forward/reverse adjacency for temporal bounds.
struct EventGraph{
    const Graph& g;
    const Query& q;
    vector<vector<int>> times;
    vector<int> offset,owner,at;
    vector<Arc> arcs;
    vector<vector<int>> fwd,rev;
    EventGraph(const Graph& graph,const Query& query,const vector<int>& active):g(graph),q(query){
        int n=(int)g.v.size();
        times.resize(n);
        offset.resize(n+1);
        for(int v=0;v<n;++v)times[v]={
            q.start,q.end
        };
        for(int i:active){
            const auto& e=g.e[i];
            times[e.from].push_back(e.dep);
            times[e.to].push_back(e.arr);
        }
        for(int v=0;v<n;++v){
            auto& t=times[v];
            sort(t.begin(),t.end());
            t.erase(unique(t.begin(),t.end()),t.end());
            offset[v+1]=offset[v]+(int)t.size();
            for(int z:t){
                owner.push_back(v);
                at.push_back(z);
            }
        }
        fwd.resize(owner.size());
        rev.resize(owner.size());
        for(int v=0;v<n;++v)for(int j=1;j<(int)times[v].size();++j)add(offset[v]+j-1,offset[v]+j,-1,0);
        for(int i:active){
            const auto& e=g.e[i];
            add(node(e.from,e.dep),node(e.to,e.arr),i,e.cost);
        }
    }
    void add(int a,int b,int e,double cost){
        int id=(int)arcs.size();
        arcs.push_back({
            a,b,e,cost
        }
        );
        fwd[a].push_back(id);
        rev[b].push_back(id);
    }
    int node(int v,int t)const{
        const auto& x=times[v];
        auto it=lower_bound(x.begin(),x.end(),t);
        if(it==x.end()||*it!=t)throw runtime_error("missing event");
        return offset[v]+(int)(it-x.begin());
    }
    int next(int v,int t)const{
        auto it=lower_bound(times[v].begin(),times[v].end(),t);
        return it==times[v].end()?-1:offset[v]+(int)(it-times[v].begin());
    }
    vector<char> reach(int start,bool reverse)const{
        vector<char> seen(owner.size());
        vector<int> todo={
            start
        };
        seen[start]=1;
        for(size_t i=0;i<todo.size();++i){
            int x=todo[i];
            for(int a:(reverse?rev[x]:fwd[x])){
                int y=reverse?arcs[a].from:arcs[a].to;
                if(!seen[y]){
                    seen[y]=1;
                    todo.push_back(y);
                }
            }
        }
        return seen;
    }
};
// Algorithm 2: reverse multi-source Dijkstra. Strict improvements point to
// previously settled successors, so even zero-cost cycles cannot trap a path.
struct Profile{
    EventGraph ev;
    vector<double> dist;
    vector<int> pointer;
    Profile(const Graph& g,const Query& q,const vector<int>& active):ev(g,q,active){
        int n=(int)ev.owner.size();
        dist.assign(n,INF);
        pointer.assign(n,-1);
        vector<char> settled(n);
        using Item=pair<double,int>;
        priority_queue<Item,vector<Item>,greater<Item>> pq;
        for(int a=ev.offset[q.target];a<ev.offset[q.target+1];++a){
            dist[a]=0;
            pq.push({
                0,a
            }
            );
        }
        while(!pq.empty()){
            auto [d,x]=pq.top();
            pq.pop();
            if(settled[x])continue;
            settled[x]=1;
            for(int a:ev.rev[x]){
                const Arc& r=ev.arcs[a];
                int y=r.from;
                double cand=d+r.cost;
                if(!settled[y]&&cand<dist[y]){
                    dist[y]=cand;
                    pointer[y]=a;
                    pq.push({
                        cand,y
                    }
                    );
                }
            }
        }
    }
    double get(int v,int t)const{
        int x=ev.next(v,t);
        return x<0?INF:dist[x];
    }
    vector<int> suffix(int v,int t)const{
        int x=ev.next(v,t);
        if(x<0||!isfinite(dist[x]))throw runtime_error("no pointer continuation");
        vector<int> path;
        for(size_t j=0;j<=pointer.size();++j){
            if(ev.owner[x]==ev.q.target)return path;
            int a=pointer[x];
            if(a<0)throw runtime_error("broken pointer");
            if(ev.arcs[a].edge>=0)path.push_back(ev.arcs[a].edge);
            x=ev.arcs[a].to;
        }
        throw runtime_error("cyclic pointer continuation");
    }
};
// Paper's two-stage safe filtering; AP rebuilds a certificate on final_edges.
struct Prepared{
    vector<int> window,time,final_edges;
    vector<vector<int>> final_out,window_out;
    vector<int> earliest,latest;
    Profile* time_profile=nullptr;
    Profile* final_profile=nullptr;
    Prepared(const Graph& g,const Query& q,bool for_ap,bool baseline=false){
        int n=(int)g.v.size();
        for(int i=0;i<(int)g.e.size();++i){
            const auto& e=g.e[i];
            if(e.dep>=q.start&&e.arr<=q.end)window.push_back(i);
        }
        window_out.assign(n,{}
        );
        for(int i:window)window_out[g.e[i].from].push_back(i);
        if(baseline)return;
        {
            EventGraph h(g,q,window);
            auto F=h.reach(h.node(q.source,q.start),false),B=h.reach(h.node(q.target,q.end),true);
            earliest.assign(n,TINF);
            latest.assign(n,-1);
            for(int x=0;x<(int)h.owner.size();++x){
                int v=h.owner[x];
                if(F[x])earliest[v]=min(earliest[v],h.at[x]);
                if(B[x])latest[v]=max(latest[v],h.at[x]);
            }
        }
        auto keep=[&](int v){
            return v==q.source||v==q.target||earliest[v]<=latest[v];
        };
        for(int i:window){
            const auto& e=g.e[i];
            if(keep(e.from)&&keep(e.to)&&e.dep>=earliest[e.from]&&e.dep<=latest[e.from])time.push_back(i);
        }
        time_profile=new Profile(g,q,time);
        for(int i:time){
            const auto& e=g.e[i];
            auto budget_keep=[&](int v){
                return v==q.source||v==q.target||time_profile->get(v,q.start)<=q.budget;
            };
            if(budget_keep(e.from)&&budget_keep(e.to))final_edges.push_back(i);
        }
        final_out.assign(n,{}
        );
        for(int i:final_edges)final_out[g.e[i].from].push_back(i);
        // Algorithm 4 uses lower_bound on each outgoing list.  Keep the
        // query-specific lists in timetable order even when the input CSV is
        // not grouped or sorted by source station.
        for(auto& out:final_out)sort(out.begin(),out.end(),[&](int a,int b){
            return g.e[a].dep!=g.e[b].dep?g.e[a].dep<g.e[b].dep:g.e[a].id<g.e[b].id;
        });
        if(for_ap)final_profile=new Profile(g,q,final_edges);
    }
    ~Prepared(){
        delete time_profile;
        delete final_profile;
    }
    Prepared(const Prepared&)=delete;
    Prepared& operator=(const Prepared&)=delete;
};
struct Route {
    vector<int> ids;
    double score=0,budget=0;
    int arrival=0;
};
bool lex_ids(const Graph& g,const vector<int>& a,const vector<int>& b){
    return lexicographical_compare(a.begin(),a.end(),b.begin(),b.end(),[&](int x,int y){
        return g.e[x].id<g.e[y].id;
    }
    );
}
bool better(const Graph& g,const Route& a,const Route& b){
    if(a.score!=b.score)return a.score>b.score;
    return lex_ids(g,a.ids,b.ids);
}
void insert_top(vector<Route>& top,Route r,int k,const Graph& g){
    for(const auto& x:top)if(x.ids==r.ids)return;
    top.push_back(move(r));
    sort(top.begin(),top.end(),[&](const Route& a,const Route& b){
        return better(g,a,b);
    }
    );
    if((int)top.size()>k)top.resize(k);
}
struct Coverage{
    vector<double> c;
    vector<int> visited;
};
void confirm(Coverage& state,int v,const Graph& g,const Query& q){
    if(find(state.visited.begin(),state.visited.end(),v)!=state.visited.end())return;
    state.visited.push_back(v);
    for(size_t i=0;i<q.keys.size();++i){
        auto it=g.v[v].cov.find(q.keys[i]);
        if(it!=g.v[v].cov.end())state.c[i]=1-(1-state.c[i])*(1-it->second);
    }
}
double score(const Coverage& a,const Query& q){
    double s=0;
    for(size_t i=0;i<q.keys.size();++i)s+=a.c[i]*q.weights[i];
    return s;
}
Coverage empty_cov(const Query& q){
    return {
        vector<double>(q.keys.size(),0),{}
    };
}
struct Eval {
    Route route;
    Coverage coverage;
};
Eval evaluate(const Graph& g,const Query& q,const vector<int>& path,bool terminal=true){
    Coverage c=empty_cov(q);
    int v=q.source,t=q.start;
    double b=0;
    for(size_t i=0;i<path.size();++i){
        const auto& e=g.e[path[i]];
        if(e.from!=v||e.dep<t||e.arr>q.end||e.dep<q.start||e.arr<e.dep||v==q.target)throw runtime_error("internal infeasible route");
        if((double)e.dep-t>=g.v[v].dwell)confirm(c,v,g,q);
        b+=e.cost;
        v=e.to;
        t=e.arr;
        if(v==q.target&&i+1<path.size())throw runtime_error("traversed target");
    }
    if(terminal){
        if(path.empty()||v!=q.target||b>q.budget)throw runtime_error("internal incomplete or over budget route");
        if((double)q.end-t>=g.v[v].dwell)confirm(c,v,g,q);
    }
    return {
        {
            path,score(c,q),b,t
        }
        ,move(c)
    };
}
vector<vector<char>> masks(const Graph& g,const Query& q,const Prepared& p){
    int n=(int)g.v.size();
    vector<vector<int>> reverse(n);
    for(int i:p.final_edges){
        const auto& e=g.e[i];
        reverse[e.to].push_back(e.from);
    }
    vector<vector<char>> m(q.keys.size(),vector<char>(n));
    for(size_t key=0;key<q.keys.size();++key){
        vector<int> todo;
        for(int v=0;v<n;++v)if(g.v[v].cov.count(q.keys[key])&&g.v[v].cov.at(q.keys[key])>0){
            m[key][v]=1;
            todo.push_back(v);
        }
        for(size_t j=0;j<todo.size();++j)for(int u:reverse[todo[j]])if(!m[key][u]){
            m[key][u]=1;
            todo.push_back(u);
        }
    }
    return m;
}
double bound(const Coverage& c,int v,const Query& q,const vector<vector<char>>& mask){
    double f=score(c,q);
    for(size_t i=0;i<q.keys.size();++i)if(mask[i][v])f+=q.weights[i]*(1-c.c[i]);
    return f;
}
struct Stats{
    uint64_t expanded=0,generated=0;
};
// Algorithm 3: preserve distinct labels and equality at the kth score.
vector<Route> exact(const Graph& g,const Query& q,const Prepared& p,Stats& st){
    vector<Route> top;
    if(p.time_profile->get(q.source,q.start)>q.budget)return top;
    auto mask=masks(g,q,p);
    struct Label{
        vector<int> path;
        Coverage c;
        int v,t;
        double b,f;
        uint64_t serial;
    };
    struct Cmp{
        bool operator()(const Label& a,const Label& b)const{
            return a.f!=b.f?a.f<b.f:a.serial>b.serial;
        }
    };
    priority_queue<Label,vector<Label>,Cmp> pq;
    uint64_t serial=0;
    pq.push({
        {}
        ,empty_cov(q),q.source,q.start,0,bound(empty_cov(q),q.source,q,mask),serial++
    }
    );
    while(!pq.empty()){
        Label s=pq.top();
        pq.pop();
        if((int)top.size()==q.k&&s.f<top.back().score)continue;
        ++st.expanded;
        if(s.v==q.target){
            insert_top(top,{
                s.path,score(s.c,q),s.b,s.t
            }
            ,q.k,g);
            continue;
        }
        for(int i:p.final_out[s.v]){
            const auto& e=g.e[i];
            if(e.dep<s.t||find(s.path.begin(),s.path.end(),i)!=s.path.end()||s.b+e.cost>q.budget)continue;
            Label a=s;
            if((double)e.dep-s.t>=g.v[s.v].dwell)confirm(a.c,s.v,g,q);
            a.path.push_back(i);
            a.v=e.to;
            a.t=e.arr;
            a.b+=e.cost;
            if(a.v==q.target&&(double)q.end-a.t>=g.v[a.v].dwell)confirm(a.c,a.v,g,q);
            if(a.b+p.time_profile->get(a.v,a.t)>q.budget)continue;
            a.f=a.v==q.target?score(a.c,q):bound(a.c,a.v,q,mask);
            if((int)top.size()==q.k&&a.f<top.back().score)continue;
            a.serial=serial++;
            pq.push(move(a));
            ++st.generated;
        }
    }
    return top;
}
// Validation comparator, not the Li/Zeng BL implementation described in paper.
vector<Route> baseline_enumeration(const Graph& g,const Query& q,const Prepared& p,Stats& st){
    vector<Route> top;
    vector<int> path;
    vector<char> used(g.e.size());
    auto walk=[&](auto&& self,int v,int t,double spent)->void{
        ++st.expanded;
        if(v==q.target){
            insert_top(top,evaluate(g,q,path).route,q.k,g);
            return;
        }
        for(int i:p.window_out[v]){
            const auto& e=g.e[i];
            if(e.dep<t||used[i]||spent+e.cost>q.budget)continue;
            used[i]=1;
            path.push_back(i);
            ++st.generated;
            self(self,e.to,e.arr,spent+e.cost);
            path.pop_back();
            used[i]=0;
        }
    };
    walk(walk,q.source,q.start,0);
    return top;
}
bool disjoint(const vector<int>& prefix,int edge,const vector<int>& tail){
    unordered_set<int> s(prefix.begin(),prefix.end());
    if(!s.insert(edge).second)return false;
    for(int i:tail)if(!s.insert(i).second)return false;
    return true;
}
vector<int> sample_ranks(int m,int r,bool multi,mt19937_64& rng){
    vector<int> ranks;
    if(r<=0)return ranks;
    if(!multi){
        for(int i=0;i<r;++i)ranks.push_back(r==1?0:(int)((int64_t)i*(m-1)/(r-1)));
    }
    else for(int i=0;i<r;++i){
        int lo=(int)((int64_t)i*m/r),hi=(int)((int64_t)(i+1)*m/r)-1;
        uniform_int_distribution<int> d(lo,hi);
        ranks.push_back(d(rng));
    }
    return ranks;
}
// Algorithm 4: each sampled choice retains a disjoint minimum-cost suffix.
Route ap_once(const Graph& g,const Query& q,const Prepared& p,int rho,int rounds,bool multi,uint64_t seed,Stats& st){
    const Profile& prof=*p.final_profile;
    vector<int> prefix,certificate=prof.suffix(q.source,q.start);
    int v=q.source,t=q.start;
    double spent=0;
    mt19937_64 rng(seed);
    for(int round=0;round<rounds&&v!=q.target;++round){
        const auto& out=p.final_out[v];
        auto it=lower_bound(out.begin(),out.end(),t,[&](int a,int z){
            return g.e[a].dep<z;
        }
        );
        int off=(int)(it-out.begin()),m=(int)out.size()-off,r=min(rho,m);
        set<int> candidates;
        for(int rank:sample_ranks(m,r,multi,rng))candidates.insert(out[off+rank]);
        if(!certificate.empty())candidates.insert(certificate[0]);
        bool found=false;
        double best_h=-INF;
        int best_arr=TINF,best_edge=-1;
        vector<int> best_tail;
        double confirmed=score(evaluate(g,q,prefix,false).coverage,q);
        for(int i:candidates){
            ++st.generated;
            const auto& e=g.e[i];
            if(e.from!=v||e.dep<t||e.arr>q.end||find(prefix.begin(),prefix.end(),i)!=prefix.end())continue;
            if(spent+e.cost+prof.get(e.to,e.arr)>q.budget)continue;
            vector<int> tail=prof.suffix(e.to,e.arr);
            if(!disjoint(prefix,i,tail))continue;
            vector<int> full=prefix;
            full.push_back(i);
            full.insert(full.end(),tail.begin(),tail.end());
            Eval value=evaluate(g,q,full);
            double denominator=1+e.cost;
            for(int j:tail)denominator+=g.e[j].cost;
            double h=(value.route.score-confirmed)/denominator;
            int arrival=value.route.arrival;
            if(!found||h>best_h||(h==best_h&&(arrival<best_arr||(arrival==best_arr&&g.e[i].id<g.e[best_edge].id)))){
                found=true;
                best_h=h;
                best_arr=arrival;
                best_edge=i;
                best_tail=move(tail);
            }
        }
        ++st.expanded;
        if(!found)break;
        prefix.push_back(best_edge);
        certificate=move(best_tail);
        spent+=g.e[best_edge].cost;
        v=g.e[best_edge].to;
        t=g.e[best_edge].arr;
    }
    prefix.insert(prefix.end(),certificate.begin(),certificate.end());
    return evaluate(g,q,prefix).route;
}
vector<Route> ap(const Graph& g,const Query& q,const Prepared& p,int rho,int rounds,int runs,Stats& st){
    vector<Route> top;
    if(p.final_profile->get(q.source,q.start)>q.budget)return top;
    for(int i=0;i<runs;++i){
        uint64_t seed=q.seed+0x9e3779b97f4a7c15ULL*(uint64_t)i;
        insert_top(top,ap_once(g,q,p,rho,rounds,runs>1,seed,st),q.k,g);
    }
    return top;
}
string escape(const string& s){
    string z="\"";
    for(char c:s){
        switch(c){
            case '"':z+="\\\"";
            break;
            case '\\':z+="\\\\";
            break;
            case '\n':z+="\\n";
            break;
            default:z+=c;
        }
    }
    return z+'"';
}
string arg(int argc,char** argv,const string& name,const string& fallback=""){
    for(int i=1;i+1<argc;i+=2)if(argv[i]==name)return argv[i+1];
    if(!fallback.empty())return fallback;
    throw runtime_error("required argument: "+name);
}
int main(int argc,char** argv){
    try{
        if((argc-1)%2)throw runtime_error("arguments must be --name value pairs");
        string stations=arg(argc,argv,"--stations"),trips=arg(argc,argv,"--trips"),queries=arg(argc,argv,"--queries"),qid=arg(argc,argv,"--query-id"),algorithm=arg(argc,argv,"--algorithm");
        int rho=stoi(arg(argc,argv,"--rho","8")),rounds=stoi(arg(argc,argv,"--rounds","20")),runs=stoi(arg(argc,argv,"--runs","20"));
        if(rho<1||rounds<0||runs<1)throw runtime_error("invalid AP parameters");
        if(algorithm!="A"&&algorithm!="AP"&&algorithm!="BL_enum")throw runtime_error("algorithm must be A, AP, or BL_enum");
        auto start=Clock::now();
        Graph g;
        g.read(stations,trips);
        Query q=read_query(queries,qid,g);
        auto loaded=Clock::now();
        Prepared prep(g,q,algorithm=="AP",algorithm=="BL_enum");
        auto preprocessed=Clock::now();
        Stats st;
        vector<Route> ans;
        if(algorithm=="A")ans=exact(g,q,prep,st);
        else if(algorithm=="AP")ans=ap(g,q,prep,rho,rounds,runs,st);
        else ans=baseline_enumeration(g,q,prep,st);
        auto finish=Clock::now();
        auto ms=[](Clock::time_point a,Clock::time_point b){
            return chrono::duration<double,milli>(b-a).count();
        };
        cout<<setprecision(17)<<"{\"query_id\":"<<escape(q.id)<<",\"algorithm\":"<<escape(algorithm)<<",\"status\":"<<escape(ans.empty()?"no_route":"ok")<<",\"runtime_ms\":"<<ms(start,finish)<<",\"load_ms\":"<<ms(start,loaded)<<",\"preprocess_ms\":"<<ms(loaded,preprocessed)<<",\"search_ms\":"<<ms(preprocessed,finish)<<",\"expanded\":"<<st.expanded<<",\"generated\":"<<st.generated<<",\"window_edges\":"<<prep.window.size()<<",\"time_edges\":"<<prep.time.size()<<",\"filtered_edges\":"<<prep.final_edges.size()<<",\"rho\":"<<rho<<",\"rounds\":"<<rounds<<",\"runs\":"<<runs<<",\"seed\":"<<q.seed<<",\"solutions\":[";
        for(size_t j=0;j<ans.size();++j){
            if(j)cout<<',';
            const auto& a=ans[j];
            cout<<"{\"edge_ids\":[";
            for(size_t z=0;z<a.ids.size();++z){
                if(z)cout<<',';
                cout<<escape(g.e[a.ids[z]].id);
            }
            cout<<"],\"score\":"<<a.score<<",\"budget\":"<<a.budget<<",\"arrival_min\":"<<a.arrival<<'}';
        }
        cout<<"]}\n";
        return 0;
    }
    catch(const exception& e){
        cerr<<"route_search: "<<e.what()<<'\n';
        return 2;
    }
}
