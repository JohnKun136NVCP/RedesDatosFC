#include <iostream>
#include <vector>

using namespace std;
using ll = long long;   // %lld
using vi = vector<ll>;

#define endl '\n'

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


