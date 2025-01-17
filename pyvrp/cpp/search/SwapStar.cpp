#include "SwapStar.h"

#include <cassert>

using pyvrp::Cost;
using pyvrp::search::Route;
using pyvrp::search::SwapStar;

void SwapStar::updateRemovalCosts(Route *R, CostEvaluator const &costEvaluator)
{
    for (size_t idx = 1; idx != R->size() + 1; ++idx)
    {
        auto const dist = DistanceSegment::merge(
            data.distanceMatrix(), R->before(idx - 1), R->after(idx + 1));

        auto const ds = DurationSegment::merge(
            data.durationMatrix(), R->before(idx - 1), R->after(idx + 1));

        auto *U = (*R)[idx];
        removalCosts(R->idx(), U->client())
            = static_cast<Cost>(dist.distance())
              - static_cast<Cost>(R->distance())
              + costEvaluator.twPenalty(ds.timeWarp(R->maxDuration()))
              - costEvaluator.twPenalty(R->timeWarp());
    }
}

void SwapStar::updateInsertionCost(Route *R,
                                   Route::Node *U,
                                   CostEvaluator const &costEvaluator)
{
    auto &insertPositions = cache(R->idx(), U->client());

    insertPositions = {};
    insertPositions.shouldUpdate = false;

    for (size_t idx = 0; idx != R->size() + 1; ++idx)
    {
        auto const dist = DistanceSegment::merge(data.distanceMatrix(),
                                                 R->before(idx),
                                                 U->route()->at(U->idx()),
                                                 R->after(idx + 1));

        auto const ds = DurationSegment::merge(data.durationMatrix(),
                                               R->before(idx),
                                               U->route()->at(U->idx()),
                                               R->after(idx + 1));

        auto const deltaCost
            = static_cast<Cost>(dist.distance())
              - static_cast<Cost>(R->distance())
              + costEvaluator.twPenalty(ds.timeWarp(R->maxDuration()))
              - costEvaluator.twPenalty(R->timeWarp());

        insertPositions.maybeAdd(deltaCost, (*R)[idx]);
    }
}

std::pair<Cost, Route::Node *> SwapStar::getBestInsertPoint(
    Route::Node *U, Route::Node *V, CostEvaluator const &costEvaluator)
{
    auto *route = V->route();
    auto &best_ = cache(route->idx(), U->client());

    if (best_.shouldUpdate)  // then we first update the insert positions
        updateInsertionCost(route, U, costEvaluator);

    for (size_t idx = 0; idx != 3; ++idx)  // only OK if V is not adjacent
        if (best_.locs[idx] && best_.locs[idx] != V && n(best_.locs[idx]) != V)
            return std::make_pair(best_.costs[idx], best_.locs[idx]);

    // As a fallback option, we consider inserting in the place of V.
    auto const dist = DistanceSegment::merge(data.distanceMatrix(),
                                             route->before(V->idx() - 1),
                                             U->route()->at(U->idx()),
                                             route->after(V->idx() + 1));

    auto const ds = DurationSegment::merge(data.durationMatrix(),
                                           route->before(V->idx() - 1),
                                           U->route()->at(U->idx()),
                                           route->after(V->idx() + 1));

    auto const deltaCost
        = static_cast<Cost>(dist.distance())
          - static_cast<Cost>(route->distance())
          + costEvaluator.twPenalty(ds.timeWarp(route->maxDuration()))
          - costEvaluator.twPenalty(route->timeWarp());

    return std::make_pair(deltaCost, p(V));
}

Cost SwapStar::evaluateMove(Route::Node *U,
                            Route::Node *V,
                            Route::Node *remove,
                            CostEvaluator const &costEvaluator)
{
    assert(V->route() == remove->route());
    assert(V != remove);

    auto const *route = V->route();

    Cost deltaCost = 0;

    if (V == p(remove))
    {
        // Special case: insert U in place of remove.
        auto const dist = DistanceSegment::merge(data.distanceMatrix(),
                                                 route->before(V->idx()),
                                                 U->route()->at(U->idx()),
                                                 route->after(V->idx() + 2));

        deltaCost += static_cast<Cost>(dist.distance());

        auto const ds = DurationSegment::merge(data.durationMatrix(),
                                               route->before(V->idx()),
                                               U->route()->at(U->idx()),
                                               route->after(V->idx() + 2));

        deltaCost += costEvaluator.twPenalty(ds.timeWarp(route->maxDuration()));

        auto const ls = LoadSegment::merge(route->before(V->idx()),
                                           U->route()->at(U->idx()),
                                           route->after(V->idx() + 2));

        deltaCost += costEvaluator.loadPenalty(ls.load(), route->capacity());
    }
    else  // in non-adjacent parts of the route.
    {
        if (V->idx() < remove->idx())
        {
            auto const dist = DistanceSegment::merge(
                data.distanceMatrix(),
                route->before(V->idx()),
                U->route()->at(U->idx()),
                route->between(V->idx() + 1, remove->idx() - 1),
                route->after(remove->idx() + 1));

            deltaCost += static_cast<Cost>(dist.distance());

            auto const ds = DurationSegment::merge(
                data.durationMatrix(),
                route->before(V->idx()),
                U->route()->at(U->idx()),
                route->between(V->idx() + 1, remove->idx() - 1),
                route->after(remove->idx() + 1));

            deltaCost
                += costEvaluator.twPenalty(ds.timeWarp(route->maxDuration()));

            auto const ls = LoadSegment::merge(
                route->before(V->idx()),
                U->route()->at(U->idx()),
                route->between(V->idx() + 1, remove->idx() - 1),
                route->after(remove->idx() + 1));

            deltaCost
                += costEvaluator.loadPenalty(ls.load(), route->capacity());
        }
        else
        {
            auto const dist = DistanceSegment::merge(
                data.distanceMatrix(),
                route->before(remove->idx() - 1),
                route->between(remove->idx() + 1, V->idx()),
                U->route()->at(U->idx()),
                route->after(V->idx() + 1));

            deltaCost += static_cast<Cost>(dist.distance());

            auto const ds = DurationSegment::merge(
                data.durationMatrix(),
                route->before(remove->idx() - 1),
                route->between(remove->idx() + 1, V->idx()),
                U->route()->at(U->idx()),
                route->after(V->idx() + 1));

            deltaCost
                += costEvaluator.twPenalty(ds.timeWarp(route->maxDuration()));

            auto const ls = LoadSegment::merge(
                route->before(remove->idx() - 1),
                route->between(remove->idx() + 1, V->idx()),
                U->route()->at(U->idx()),
                route->after(V->idx() + 1));

            deltaCost
                += costEvaluator.loadPenalty(ls.load(), route->capacity());
        }
    }

    deltaCost -= static_cast<Cost>(route->distance());
    deltaCost -= costEvaluator.twPenalty(route->timeWarp());
    deltaCost -= costEvaluator.loadPenalty(route->load(), route->capacity());

    return deltaCost;
}

void SwapStar::init(Solution const &solution)
{
    LocalSearchOperator<Route>::init(solution);
    std::fill(updated.begin(), updated.end(), true);
}

Cost SwapStar::evaluate(Route *routeU,
                        Route *routeV,
                        CostEvaluator const &costEvaluator)
{
    best = {};

    for (auto *route : {routeU, routeV})
        if (updated[route->idx()])
        {
            updateRemovalCosts(route, costEvaluator);
            updated[route->idx()] = false;

            for (size_t idx = data.numDepots(); idx != data.numLocations();
                 ++idx)
                cache(route->idx(), idx).shouldUpdate = true;
        }

    for (auto *U : *routeU)
        for (auto *V : *routeV)
        {
            Cost deltaCost = 0;

            // Approximates the actual load changes - this is far from exact
            // when there are also pickups. So it's pretty rough, but fast and
            // seems to work well enough for most instances.
            ProblemData::Client const &uClient = data.location(U->client());
            ProblemData::Client const &vClient = data.location(V->client());
            auto const uLoad = std::max(uClient.delivery, uClient.pickup);
            auto const vLoad = std::max(vClient.delivery, vClient.pickup);
            auto const loadDiff = uLoad - vLoad;

            deltaCost += costEvaluator.loadPenalty(routeU->load() - loadDiff,
                                                   routeU->capacity());
            deltaCost -= costEvaluator.loadPenalty(routeU->load(),
                                                   routeU->capacity());

            deltaCost += costEvaluator.loadPenalty(routeV->load() + loadDiff,
                                                   routeV->capacity());
            deltaCost -= costEvaluator.loadPenalty(routeV->load(),
                                                   routeV->capacity());

            deltaCost += removalCosts(routeU->idx(), U->client());
            deltaCost += removalCosts(routeV->idx(), V->client());

            if (deltaCost >= 0)  // an early filter on many moves, before doing
                continue;        // costly work determining insertion points

            auto [extraV, UAfter] = getBestInsertPoint(U, V, costEvaluator);
            deltaCost += extraV;

            if (deltaCost >= 0)  // continuing here avoids evaluating another
                continue;        // potentially costly insertion point below

            auto [extraU, VAfter] = getBestInsertPoint(V, U, costEvaluator);
            deltaCost += extraU;

            if (deltaCost < best.cost)
            {
                best.cost = deltaCost;

                best.U = U;
                best.UAfter = UAfter;

                best.V = V;
                best.VAfter = VAfter;
            }
        }

    // It is possible for positive delta costs to turn negative when we do a
    // complete evaluation. But in practice that almost never happens, and is
    // not worth spending time on.
    if (best.cost >= 0)
        return best.cost;

    // Now do an exact evaluation of the proposed swap move. This includes
    // possible time warp penalties.
    return evaluateMove(best.V, best.VAfter, best.U, costEvaluator)
           + evaluateMove(best.U, best.UAfter, best.V, costEvaluator);
}

void SwapStar::apply(Route *U, Route *V) const
{
    assert(best.U);
    assert(best.UAfter);
    assert(best.V);
    assert(best.VAfter);

    U->remove(best.U->idx());
    V->remove(best.V->idx());

    V->insert(best.UAfter->idx() + 1, best.U);
    U->insert(best.VAfter->idx() + 1, best.V);
}

void SwapStar::update(Route *U) { updated[U->idx()] = true; }
