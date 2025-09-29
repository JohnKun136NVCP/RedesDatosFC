#include <iostream>
#include <vector>
#include <utility>
#include <algorithm>
#include <numeric>
#include <random>
#include <chrono>
using namespace std;
using ll = long long;   // %lld
using ld = long double; // %0.9Lf
using vi = vector<ll>;
using pi = pair<ll,ll>;

#define endl '\n'
#define fst first
#define snd second
#define pb push_back
#define FOR(i,a,b) for(ll i = (a); i < (b); i++)
#define RFOR(i,a,b) for(ll i = (b)-1; i >= (a); i--)
#define all(a) (a).begin(), (a).end()
#define rall(a) (a).rbegin(), (a).rend()

const ll MOD = 1e9+7; // 998244353
const ll MXN = 2e5+5;
const ll INF = 1e18;

mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());

// Classic unbouded Knapsack DP
ll countWays(ll target, vi& coins) {
    vi dp(target + 1, 0);
    dp[0] = 1; // Caso base, 1 manera de crear valor 0
    for(auto coin : coins) {
        for(ll s = coin; s <= target; s++) {
            dp[s] += dp[s - coin];
        }
    }
    return dp[target];
}

int main() {
    ios::sync_with_stdio(0); cin.tie(0); cout.tie(0);
    vi coins = {1, 2, 5, 10, 20, 50, 100, 200};
    ll target = 200;
    cout << countWays(target, coins) << endl;

    return 0;
}


