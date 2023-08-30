#include "primitives.h"
#include "TimeWindowSegment.h"

#include <cassert>

using pyvrp::Cost;
using TWS = pyvrp::TimeWindowSegment;

Cost pyvrp::search::insertCost(Route::Node *U,
                               Route::Node *V,
                               ProblemData const &data,
                               CostEvaluator const &costEvaluator)
{
    if (!V->route() || U->isDepot())
        return 0;

    auto *route = V->route();

    Distance const deltaDist = data.dist(V->client(), U->client())
                               + data.dist(U->client(), n(V)->client())
                               - data.dist(V->client(), n(V)->client());

    auto const &uClient = data.client(U->client());
    Cost deltaCost = static_cast<Cost>(deltaDist) - uClient.prize;

    deltaCost += costEvaluator.loadPenalty(route->load() + uClient.demand,
                                           route->capacity());
    deltaCost -= costEvaluator.loadPenalty(route->load(), route->capacity());

    auto const vTWS = TWS::merge(data.durationMatrix(),
                                 route->twsBefore(V->idx()),
                                 TWS(U->client(), uClient),
                                 route->twsAfter(V->idx() + 1));

    deltaCost += costEvaluator.twPenalty(vTWS.totalTimeWarp());
    deltaCost -= costEvaluator.twPenalty(route->timeWarp());

    return deltaCost;
}

Cost pyvrp::search::removeCost(Route::Node *U,
                               ProblemData const &data,
                               CostEvaluator const &costEvaluator)
{
    if (!U->route() || U->isDepot())
        return 0;

    auto *route = U->route();

    Distance const deltaDist = data.dist(p(U)->client(), n(U)->client())
                               - data.dist(p(U)->client(), U->client())
                               - data.dist(U->client(), n(U)->client());

    auto const &uClient = data.client(U->client());
    Cost deltaCost = static_cast<Cost>(deltaDist) + uClient.prize;

    deltaCost += costEvaluator.loadPenalty(route->load() - uClient.demand,
                                           route->capacity());
    deltaCost -= costEvaluator.loadPenalty(route->load(), route->capacity());

    auto uTWS = TWS::merge(data.durationMatrix(),
                           route->twsBefore(U->idx() - 1),
                           route->twsAfter(U->idx() + 1));

    deltaCost += costEvaluator.twPenalty(uTWS.totalTimeWarp());
    deltaCost -= costEvaluator.twPenalty(route->timeWarp());

    return deltaCost;
}