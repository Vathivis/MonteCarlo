#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <map>
#include <algorithm>
#include <tuple>
#include <functional>
#include <iomanip>
#include <random>
#ifdef _OPENMP
#include <omp.h>
#endif

#define ITERATIONS 1'000'000

using namespace std;

unsigned int bounded_rand(unsigned range, std::mt19937& rng) {
	std::uniform_int_distribution<unsigned int> dist(0, range - 1);
	return dist(rng);
}

static void shuffle_in_place(vector<string>& items, std::mt19937& rng) {
	for (size_t i = items.size(); i > 1; --i) {
		std::uniform_int_distribution<size_t> dist(0, i - 1);
		size_t j = dist(rng);
		swap(items[i - 1], items[j]);
	}
}

static vector<string> break_tie_sov(const vector<string>& group,
	const map<string, int>& sov,
	int start_rank,
	int cutoff_index,
	bool& coinflip_affects_cutoff,
	std::mt19937& rng) {
	map<int, vector<string>, greater<int>> buckets;
	for (const auto& team : group) {
		auto it = sov.find(team);
		int score = (it == sov.end()) ? 0 : it->second;
		buckets[score].push_back(team);
	}

	vector<string> resolved;
	int current_rank = start_rank;
	for (auto& entry : buckets) {
		auto& bucket = entry.second;
		if (bucket.size() > 1) {
			if (cutoff_index >= 0 &&
				cutoff_index >= current_rank &&
				cutoff_index < current_rank + static_cast<int>(bucket.size())) {
				coinflip_affects_cutoff = true;
			}
			shuffle_in_place(bucket, rng);
		}
		resolved.insert(resolved.end(), bucket.begin(), bucket.end());
		current_rank += static_cast<int>(bucket.size());
	}
	return resolved;
}

static vector<string> break_tie_h2h(const vector<string>& group,
	const map<string, map<string, int>>& h2h,
	const map<string, int>& sov,
	int start_rank,
	int cutoff_index,
	bool& coinflip_affects_cutoff,
	std::mt19937& rng) {
	map<int, vector<string>, greater<int>> buckets;
	for (const auto& team : group) {
		int score = 0;
		auto it = h2h.find(team);
		if (it != h2h.end()) {
			for (const auto& other : group) {
				if (other == team) {
					continue;
				}
				auto it2 = it->second.find(other);
				if (it2 != it->second.end()) {
					score += it2->second;
				}
			}
		}
		buckets[score].push_back(team);
	}

	vector<string> resolved;
	int current_rank = start_rank;
	for (auto& entry : buckets) {
		auto& bucket = entry.second;
		if (bucket.size() == 1) {
			resolved.push_back(bucket.front());
			current_rank++;
			continue;
		}
		auto sov_resolved = break_tie_sov(bucket, sov, current_rank, cutoff_index, coinflip_affects_cutoff, rng);
		resolved.insert(resolved.end(), sov_resolved.begin(), sov_resolved.end());
		current_rank += static_cast<int>(bucket.size());
	}
	return resolved;
}

vector<pair<string,int>> resolve_ties(const vector<string>& teams,
	const vector<tuple<string, string, string>>& played,
	const vector<tuple<string, string, string>>& guesses,
	int cutoff,
	bool& coinflip_affects_cutoff,
	std::mt19937& rng) {
	vector<tuple<string, string, string>> all_games = played;
	all_games.insert(all_games.end(), guesses.begin(), guesses.end());

	map<string, int> wins;
	for (const auto& team : teams) {
		wins[team] = 0;
	}

	map<string, map<string, int>> h2h;
	for (const auto& game : all_games) {
		const auto& team1 = get<0>(game);
		const auto& team2 = get<1>(game);
		const auto& winner = get<2>(game);
		if (winner.empty()) {
			continue;
		}
		string loser = (winner == team1) ? team2 : team1;
		wins[winner]++;
		h2h[winner][loser]++;
	}

	map<string, int> sov;
	for (const auto& team : teams) {
		sov[team] = 0;
	}
	for (const auto& game : all_games) {
		const auto& team1 = get<0>(game);
		const auto& team2 = get<1>(game);
		const auto& winner = get<2>(game);
		if (winner.empty()) {
			continue;
		} 
		string loser = (winner == team1) ? team2 : team1;
		sov[winner] += wins[loser];
	}

	map<int, vector<string>, greater<int>> win_groups;
	for (const auto& team : teams) {
		win_groups[wins[team]].push_back(team);
	}

	int cutoff_index = cutoff - 1;
	if (cutoff <= 0 || cutoff_index >= static_cast<int>(teams.size())) {
		cutoff_index = -1;
	}

	vector<pair<string,int>> ordered;
	int current_rank = 0;
	for (auto& entry : win_groups) {
		auto& group = entry.second;
		if (group.size() == 1) {
			ordered.emplace_back(group.front(), entry.first);
			current_rank++;
			continue;
		}
		auto resolved = break_tie_h2h(group, h2h, sov, current_rank, cutoff_index, coinflip_affects_cutoff, rng);
		for (const auto& team : resolved) {
			ordered.emplace_back(team, wins[team]);
		}
		current_rank += static_cast<int>(group.size());
	}
	return ordered;
}

void run_league_mc() {
	cout << "Loading data..." << endl;
	
	vector<tuple<string, string, string>> played;
	vector<tuple<string, string, string>> remaining;
	vector<string> teams;
	map<string, int> teamsInPlayoffs;

	ifstream games;
	games.open("games.txt");
	vector<string> lineParts;
	string line;
	while (getline(games, line)) {
		if (line.empty() || line[0] == '#') {
			continue;
		}

		string part;
		stringstream ss(line);
		while (getline(ss, part, ',')) {
			lineParts.emplace_back(part);
		}

		if (lineParts[0] == "GAME" && lineParts.size() > 3 && !lineParts[3].empty()) {
			played.emplace_back(lineParts[1], lineParts[2], lineParts[3]);
		} else if (lineParts[0] == "GAME") {
			remaining.emplace_back(lineParts[1], lineParts[2], "");
		} else if (lineParts[0] == "TEAM") {
			teams.emplace_back(lineParts[1]);
			teamsInPlayoffs.emplace(lineParts[1], 0);
		}

		lineParts.clear();
	}
	
	cout << "Running league Monte Carlo simulation..." << endl;
	unsigned int base_seed = static_cast<unsigned int>(time(nullptr));
	int thread_count = 1;
#ifdef _OPENMP
	thread_count = omp_get_max_threads();
#endif

	vector<map<string, int>> localCounts(thread_count);
	vector<long long> localCoinflips(thread_count, 0);
	for (int t = 0; t < thread_count; ++t) {
		for (const auto& team : teams) {
			localCounts[t][team] = 0;
		}
	}

#ifdef _OPENMP
#pragma omp parallel
#endif
	{
		int tid = 0;
#ifdef _OPENMP
		tid = omp_get_thread_num();
#endif
		std::mt19937 rng(base_seed ^ static_cast<unsigned int>(tid * 0x9e3779b9U));

#ifdef _OPENMP
#pragma omp for
#endif
		for (int it = 0; it < ITERATIONS; ++it) {
			vector<tuple<string, string, string>> guesses = remaining;
			for (int i = 0; i < guesses.size(); ++i) {
				unsigned int win = bounded_rand(2, rng);
				if (win) {
					get<2>(guesses.at(i)) = get<0>(guesses.at(i));
				} else {
					get<2>(guesses.at(i)) = get<1>(guesses.at(i));
				}
			}

			bool coinflip_affects_cutoff = false;
			auto res = resolve_ties(teams, played, guesses, 8, coinflip_affects_cutoff, rng);
			int cutoff = 8;
			if (res.size() < static_cast<size_t>(cutoff)) {
				cutoff = static_cast<int>(res.size());
			}
			for (int j = 0; j < cutoff; ++j) {
				localCounts[tid][res[j].first]++;
			}
			if (coinflip_affects_cutoff) {
				localCoinflips[tid]++;
			}
		}
	}

	long long coinflips_affecting_cutoff = 0;
	for (int t = 0; t < thread_count; ++t) {
		for (const auto& entry : localCounts[t]) {
			teamsInPlayoffs[entry.first] += entry.second;
		}
		coinflips_affecting_cutoff += localCoinflips[t];
	}
	
	cout << "Playoffs chances:" << endl;
	vector<pair<string, int>> ordered(teamsInPlayoffs.begin(), teamsInPlayoffs.end());
	sort(ordered.begin(), ordered.end(),
		[](const auto& a, const auto& b) {
			if (a.second != b.second) {
				return a.second > b.second;
			}
			return a.first < b.first;
		}
	);

	cout << left << setw(22) << "Team" << right << setw(10) << "Count" << setw(12) << "Percent" << endl;
	cout << string(44, '-') << endl;
	for (const auto& entry : ordered) {
		double pct = (static_cast<double>(entry.second) * 100.0) / static_cast<double>(ITERATIONS);
		cout << left << setw(22) << entry.first
			<< right << setw(10) << entry.second
			<< setw(11) << fixed << setprecision(2) << pct << "%" << endl;
	}
	cout << endl;
	cout << "Coinflip tiebreaks affecting top-8: " << coinflips_affecting_cutoff << endl;
	

	return;
}
