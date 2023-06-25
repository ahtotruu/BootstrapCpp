#include <cassert>
#include <concepts>
#include <coroutine>
#include <iostream>
#include <optional>
#include <functional>
#include <map>
#include <random>
#include <string>
#include <tuple> 
#include <unordered_map>
#include <utility>


// Listing 8.1 Read an optional zero or one
std::optional<int> read_number(std::istream& in)
{
    std::string line;
    std::getline(in, line);
    if (line == "0") {
        return { 0 };
    }
    else if (line == "1") {
        return { 1 };
    }
    return {};
}

// Listing 8.2 A pennies game
void pennies_game()
{
    int player_wins = 0;
    int turns = 0;
    std::mt19937 gen{ std::random_device{}() };
    std::uniform_int_distribution dist(0, 1);

    std::cout << "Select 0 or 1 at random and press enter.\n";
    std::cout << "If the computer predicts your guess it wins.\n";
    while (true)
    {
        const int prediction = dist(gen);

        auto input = read_number(std::cin);
        if (!input)
        {
            break;
        }
        int player_choice = input.value();

        ++turns;
        std::cout << "You pressed " << player_choice << ", I guessed " << prediction << '\n';

        if (player_choice != prediction)
        {
            ++player_wins;
        }
    }
    std::cout << "you win " << player_wins << '\n'
        << "I win " << turns - player_wins << '\n';
}

// Listing 8.3 Three possible choices
enum class Choice
{
    Same,
    Change,
    Shrug,
};

using state_t = std::tuple<int, Choice, int>;
using last_choices_t = std::pair<Choice, Choice>;

// Listing 8.5 Specializing std::hash for our state tuple
template<>
struct std::hash<state_t>
{
    std::size_t operator()(state_t const& state) const noexcept
    {
        std::size_t h1 = std::hash<int>{}(std::get<0>(state));
        std::size_t h2 = std::hash<Choice>{}(std::get<1>(state));
        std::size_t h3 = std::hash<int>{}(std::get<2>(state));
        return h1 + (h2 << 1) + (h3 << 2);
    }
};

// Listing 8.6 An initial state table
std::unordered_map<state_t, last_choices_t> initial_state()
{
    const auto unset = std::pair<Choice, Choice>{ Choice::Shrug, Choice::Shrug };
    return {
        { {0,Choice::Same,0}, unset },
        { {0,Choice::Same,1}, unset },
        { {0,Choice::Change,0}, unset },
        { {0,Choice::Change,1}, unset },
        { {1,Choice::Same,0}, unset },
        { {1,Choice::Same,1}, unset },
        { {1,Choice::Change,0}, unset },
        { {1,Choice::Change,1}, unset },
    };
}

// Listing 8.8 Choice from state
Choice prediction_method(const last_choices_t& choices)
{
    if (choices.first == choices.second)
    {
        return choices.first;
    }
    else
    {
        return Choice::Shrug;
    }
}
// Listing 8.9 Class to track the game's state
class State
{
    std::unordered_map<state_t, last_choices_t> state_lookup = initial_state();
public:
    // Listing 8.10 Find the choices or return two shrugs
    last_choices_t choices(const state_t& key) const
    {
        if (auto it = state_lookup.find(key); it!=state_lookup.end())
        {
            return it->second;
        }
        else
        {
            return { Choice::Shrug, Choice::Shrug };
        }
    }
    // Listing 8.11 Update choices for valid keys
    void update(const state_t& key, const Choice turn_changed)
    {
        if (auto it = state_lookup.find(key); it != state_lookup.end())
        {
            const auto [prev2, prev1] = it->second;
            last_choices_t value{ prev1, turn_changed };
            state_lookup[key] = value;
        }
    }
};

// Listing 8.12 A mind-reading class
template <std::invocable<> T, typename U>
class MindReader {
    State state_table;

    T generator;
    U distribution;

    int prediction = flip();

    int prev_win_or_loss2 = -1;
    Choice previous_turn_changed = Choice::Shrug;
    int prev_win_or_loss1 = -1;
    int previous_go = -1;

public:
    MindReader(T gen, U dis)
        : generator(gen), distribution(dis)
    {
    }

    int get_prediction()
    {
        return prediction;
    }

    // Listing 8.13 The mind reader's update method
    bool update(int player_choice)
    {
        bool guessing = false;
        const Choice turn_changed = player_choice == previous_go ? Choice::Same : Choice::Change;
        previous_go = player_choice;

        state_table.update({ prev_win_or_loss2, previous_turn_changed, prev_win_or_loss1 }, turn_changed);

        const bool player_won_this_turn = player_choice != prediction;
        prev_win_or_loss2 = prev_win_or_loss1;
        previous_turn_changed = turn_changed;
        prev_win_or_loss1 = player_won_this_turn;

        auto option = prediction_method(state_table.choices({ prev_win_or_loss2, previous_turn_changed, prev_win_or_loss1 }));
        switch (option)
        {
        case Choice::Shrug:
            prediction = flip();
            guessing = true;
            break;
        case Choice::Change:
            prediction = player_choice ^ 1;
            break;
        case Choice::Same:
            prediction = player_choice;
            break;
        }
        return guessing;
    }

    int flip()
    {
        return distribution(generator);
    }
};

// Listing 8.7 Check we have no hash collisions (and more besides)
void check_properties()
{
    // No bucket clashes
    std::unordered_map<state_t, last_choices_t> states = initial_state();
    for (size_t bucket = 0; bucket < states.bucket_count(); bucket++)
    {
        auto bucket_size = states.bucket_size(bucket);
        assert(bucket_size <= 1);
    }

    {
        MindReader mr([]() { return 0; }, [](auto gen) { return gen(); });
        assert(mr.update(0)); // guesses first
        assert(mr.update(0)); // second is a guess too
    }

    assert(prediction_method({ Choice::Shrug, Choice::Shrug }) == Choice::Shrug);
    assert(prediction_method({ Choice::Shrug, Choice::Change }) == Choice::Shrug);
    assert(prediction_method({ Choice::Shrug, Choice::Same }) == Choice::Shrug);
    assert(prediction_method({ Choice::Change, Choice::Shrug }) == Choice::Shrug);
    assert(prediction_method({ Choice::Same, Choice::Shrug }) == Choice::Shrug);
    assert(prediction_method({ Choice::Change, Choice::Change }) == Choice::Change);
    assert(prediction_method({ Choice::Same, Choice::Same }) == Choice::Same);
    assert(prediction_method({ Choice::Change, Choice::Same }) == Choice::Shrug);
    assert(prediction_method({ Choice::Same, Choice::Change }) == Choice::Shrug);

    State s;
    auto got1 = s.choices({ -1, Choice::Shrug, -1 });
    assert(got1.first == Choice::Shrug);
    assert(got1.second == Choice::Shrug);
    auto got2 = s.choices({ 0, Choice::Same, -1 });
    assert(got2.first == Choice::Shrug);
    assert(got2.second == Choice::Shrug);
    auto got3 = s.choices({ 1, Choice::Change, -1 });
    assert(got3.first == Choice::Shrug);
    assert(got3.second == Choice::Shrug);

    {
        // Listing 6.12 had a RandomBlob we tested
        // This always returns 0
        MindReader mr([]() { return 0; }, [](auto gen) { return gen(); });
        assert( mr.update(0)); //flip first two goes
        assert( mr.update(0)); //flip first two goes
        // The random generator always returns zero
        // it guesses first
        // state is 
        // lose, same, lose
        // but without two matching next choices
        assert( mr.update(0));
        // now 
        // lose, same, lose -> -1 ,lose
        // so when we decide 0 it stops guessing
        // now the state is
        // lose, same, lose -> lose ,lose
        // so it will predict a 0 next
        assert(!mr.update(0));
        assert( mr.get_prediction() == 0);
        assert(!mr.update(0));
        assert( mr.get_prediction() == 0);
        assert(!mr.update(0));
        assert( mr.get_prediction() == 0);
        assert(!mr.update(0));
        assert( mr.get_prediction() == 0);
        assert(!mr.update(0));
        assert( mr.get_prediction() == 0);
    }
}

// Listing 8.14 A mind reading game
void mind_reader()
{
    int turns = 0;
    int player_wins = 0;
    int guessing = 0;

    std::mt19937 gen{ std::random_device{}() };
    std::uniform_int_distribution dist{ 0, 1 };
    MindReader mr(gen, dist);

    std::cout << "Select 0 or 1 at random and press enter.\n";
    std::cout << "If the computer predicts your guess it wins\n";
    std::cout << "and it can now read your mind.\n";
    while (true)
    {
        int prediction = mr.get_prediction();

        auto input = read_number(std::cin);
        if (!input)
        {
            break;
        }
        int player_choice = input.value();

        ++turns;
        std::cout << "You pressed " << player_choice 
            << ", I guessed " << prediction << '\n';

        if (player_choice != prediction)
        {
            ++player_wins;
        }
        if (mr.update(player_choice))
        {
            ++guessing;
        }
    }
    std::cout << "you win " << player_wins << '\n'
        << "machine guessed " << guessing << " times" << '\n'
        << "machine won " << (turns - player_wins) << '\n';
}

// Listing 8.17 and 8.19 The coroutine's Task and promise_type
struct Task
{
    struct promise_type
    {
        std::pair<int, int> choice_and_prediction;

        Task get_return_object()
        {
            return {
              .handle = std::coroutine_handle<promise_type>::from_promise(*this)
            };
        }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void unhandled_exception() {}
        std::suspend_always yield_value(std::pair<int, int> got)
        {
            choice_and_prediction = got;
            return {};
        }

        void return_void() { }
    };

    bool done() { return handle.done(); }
    std::pair<int, int> choice_and_prediction()
    {
        return handle.promise().choice_and_prediction;
    }
    void next() { handle(); }
    ~Task() { handle.destroy(); }
    std::coroutine_handle<promise_type> handle;
};

// Listing 8.16 Our first coroutine
Task coroutine_game()
{
    std::mt19937 gen{ std::random_device{}() };
    std::uniform_int_distribution dist{ 0, 1 };
    MindReader mr(gen, dist);
    while (true)
    {
        auto input = read_number(std::cin);
        if (!input)
        {
            co_return;
        }
        int player_choice = input.value();
        co_yield{ player_choice , mr.get_prediction() }; // We could actually co yield dist(gen) first two callls
        mr.update(player_choice);
    }
}

// Listing 8.20 A coroutine version of a mind reader
void coroutine_minder_reader()
{
    int turns = 0;
    int player_wins = 0;

    std::cout << "Select 0 or 1 at random and press enter.\n";
    std::cout << "If the computer predicts your guess it wins\nand it can now read your mind.\n";

    Task game = coroutine_game();

    while (!game.done())
    {
        auto [player_choice, prediction] = game.choice_and_prediction();
        ++turns;
        std::cout << "You pressed " << player_choice << ", I guessed " << prediction << '\n';

        if (player_choice != prediction)
        {
            ++player_wins;
        }
        game.next();
    }
    std::cout << "you win " << player_wins << '\n'
        << "machine won " << (turns - player_wins) << '\n';
}

int main()
{
    check_properties();

    pennies_game();
    // Choose one version of the mind reader (or play both if you want):
    mind_reader();
    //coroutine_minder_reader();
}