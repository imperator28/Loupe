#include "app/render/SectionMeshBuilder.h"
#include <vector>
#include <array>
#include <cstdint>
#include <unordered_map>

#include <QHash>
#include <QSet>
#include <QVector2D>

#include <algorithm>
#include <cmath>
#include <limits>

namespace loupe::app::render {

namespace {

struct Segment final {
    QVector3D start;
    QVector3D end;
};

struct Contour final {
    QVector<QVector3D> points;
    bool closed{};
};

struct EndpointNode final {
    QVector3D point;
    QVector<int> edgeIndices;
};

struct IndexedSegment final {
    int first{};
    int second{};
};

// A grid cell of the endpoint weld. Integer keys: formatting each cell as a string was
// most of the time a dense section took to build.
struct CellKey final {
    qint64 x{};
    qint64 y{};
    qint64 z{};
    bool operator==(const CellKey&) const = default;
};

struct CellKeyHash final {
    std::size_t operator()(const CellKey& key) const noexcept
    {
        auto hash = static_cast<std::uint64_t>(key.x) * 0x9E3779B97F4A7C15ULL;
        hash ^= static_cast<std::uint64_t>(key.y) * 0xC2B2AE3D27D4EB4FULL + (hash << 6U) + (hash >> 2U);
        hash ^= static_cast<std::uint64_t>(key.z) * 0x165667B19E3779F9ULL + (hash << 6U) + (hash >> 2U);
        return static_cast<std::size_t>(hash);
    }
};

CellKey cellKey(const qint64 x, const qint64 y, const qint64 z)
{
    return {x, y, z};
}

float sourceDiagonal(const QVector<float>& vertices)
{
    QVector3D minimum(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                      std::numeric_limits<float>::max());
    QVector3D maximum(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
                      std::numeric_limits<float>::lowest());
    for (qsizetype index = 0; index + 2 < vertices.size(); index += 3) {
        const QVector3D point{vertices.at(index), vertices.at(index + 1), vertices.at(index + 2)};
        minimum.setX(std::min(minimum.x(), point.x()));
        minimum.setY(std::min(minimum.y(), point.y()));
        minimum.setZ(std::min(minimum.z(), point.z()));
        maximum.setX(std::max(maximum.x(), point.x()));
        maximum.setY(std::max(maximum.y(), point.y()));
        maximum.setZ(std::max(maximum.z(), point.z()));
    }
    return vertices.isEmpty() ? 0.0F : (maximum - minimum).length();
}

QVector<Contour> assembleContours(const QVector<Segment>& segments, const float tolerance)
{
    if (segments.isEmpty()) return {};

    const auto toleranceSquared = tolerance * tolerance;
    const auto gridCoordinate = [tolerance](const float value) {
        return static_cast<qint64>(std::floor(value / tolerance));
    };

    QVector<EndpointNode> nodes;
    std::unordered_map<CellKey, QVector<int>, CellKeyHash> nodesByCell;
    const auto nodeForPoint = [&nodes, &nodesByCell, &gridCoordinate, toleranceSquared](const QVector3D& point) {
        const auto cellX = gridCoordinate(point.x());
        const auto cellY = gridCoordinate(point.y());
        const auto cellZ = gridCoordinate(point.z());
        auto closest = -1;
        auto closestDistance = toleranceSquared;
        for (qint64 x = cellX - 1; x <= cellX + 1; ++x) {
            for (qint64 y = cellY - 1; y <= cellY + 1; ++y) {
                for (qint64 z = cellZ - 1; z <= cellZ + 1; ++z) {
                    const auto found = nodesByCell.find(cellKey(x, y, z));
                    if (found == nodesByCell.end()) continue;
                    for (const auto candidate : found->second) {
                        const auto distance = (nodes.at(candidate).point - point).lengthSquared();
                        if (distance <= closestDistance) {
                            closest = candidate;
                            closestDistance = distance;
                        }
                    }
                }
            }
        }
        if (closest >= 0) return closest;
        const auto index = static_cast<int>(nodes.size());
        nodes.append({point, {}});
        nodesByCell[cellKey(cellX, cellY, cellZ)].append(index);
        return index;
    };

    QVector<IndexedSegment> indexedSegments;
    QSet<quint64> uniqueSegments;
    for (const auto& segment : segments) {
        const auto first = nodeForPoint(segment.start);
        const auto second = nodeForPoint(segment.end);
        if (first == second) continue;
        const auto minimum = std::min(first, second);
        const auto maximum = std::max(first, second);
        const auto key = (static_cast<quint64>(static_cast<quint32>(minimum)) << 32U)
                | static_cast<quint32>(maximum);
        if (uniqueSegments.contains(key)) continue;
        uniqueSegments.insert(key);
        const auto edgeIndex = static_cast<int>(indexedSegments.size());
        indexedSegments.append({first, second});
        nodes[first].edgeIndices.append(edgeIndex);
        nodes[second].edgeIndices.append(edgeIndex);
    }

    QVector<Contour> contours;
    QVector<bool> used(indexedSegments.size(), false);
    const auto otherNode = [&indexedSegments](const int edgeIndex, const int node) {
        const auto& edge = indexedSegments.at(edgeIndex);
        return edge.first == node ? edge.second : edge.first;
    };
    for (int initialEdge = 0; initialEdge < indexedSegments.size(); ++initialEdge) {
        if (used.at(initialEdge)) continue;
        const auto& firstEdge = indexedSegments.at(initialEdge);
        auto start = firstEdge.first;
        if (nodes.at(firstEdge.second).edgeIndices.size() == 1) start = firstEdge.second;
        auto previous = start;
        auto current = otherNode(initialEdge, start);
        used[initialEdge] = true;

        Contour contour;
        contour.points.append(nodes.at(start).point);
        contour.points.append(nodes.at(current).point);
        while (contour.points.size() <= indexedSegments.size() + 1) {
            auto nextEdge = -1;
            for (const auto candidate : nodes.at(current).edgeIndices) {
                if (used.at(candidate)) continue;
                if (otherNode(candidate, current) != previous) {
                    nextEdge = candidate;
                    break;
                }
                if (nextEdge < 0) nextEdge = candidate;
            }
            if (nextEdge < 0) break;
            used[nextEdge] = true;
            const auto next = otherNode(nextEdge, current);
            if (next == start) {
                contour.closed = true;
                break;
            }
            contour.points.append(nodes.at(next).point);
            previous = current;
            current = next;
        }
        if (contour.points.size() >= 2) contours.append(std::move(contour));
    }
    return contours;
}

// Drops repeated points and points on a straight run, in one pass: a vertex goes when it
// lies within `tolerance` of the line through its kept neighbours. Restarting the scan after
// every removal made a finely tessellated circle cost the cube of its point count.
void simplifyContour(QVector<QVector3D>* points, const QVector3D& normal, const float tolerance, const bool closed)
{
    if (!points || points->size() < 3) return;
    const auto toleranceSquared = tolerance * tolerance;
    const auto straight = [&normal, tolerance](const QVector3D& previous, const QVector3D& current, const QVector3D& next) {
        const auto before = current - previous;
        const auto after = next - current;
        const auto length = before.length() + after.length();
        const auto turn = std::abs(QVector3D::dotProduct(QVector3D::crossProduct(before, after), normal));
        return length > 0.0F && turn <= tolerance * length;
    };
    QVector<QVector3D> kept;
    kept.reserve(points->size());
    for (const auto& point : std::as_const(*points)) {
        if (!kept.isEmpty() && (point - kept.constLast()).lengthSquared() <= toleranceSquared) continue;
        while (kept.size() >= 2 && straight(kept.at(kept.size() - 2), kept.constLast(), point)) kept.removeLast();
        kept.append(point);
    }
    if (closed) {
        if (kept.size() > 2 && (kept.constFirst() - kept.constLast()).lengthSquared() <= toleranceSquared) kept.removeLast();
        // The seam: the run may continue through the first point.
        while (kept.size() > 3 && straight(kept.at(kept.size() - 2), kept.constLast(), kept.constFirst())) kept.removeLast();
        while (kept.size() > 3 && straight(kept.constLast(), kept.constFirst(), kept.at(1))) kept.removeFirst();
    }
    *points = std::move(kept);
}

// Joins chains the cut left open. A part's mesh is one surface only if every face shares its
// boundary nodes with its neighbours; a face tessellated on its own, a sliver the mesher
// dropped, or a seam can leave a gap, and an open chain cannot be filled, so the cap would
// show as a hollow outline. Ends closer than `gap` are joined, nearest first; anything
// farther apart is a real opening and stays open.
void stitchOpenContours(QVector<Contour>* contours, const float gap)
{
    if (!contours || gap <= 0.0F) return;
    const auto gapSquared = gap * gap;
    bool merged = true;
    while (merged) {
        merged = false;
        for (auto& contour : *contours) {
            if (contour.closed || contour.points.size() < 3) continue;
            if ((contour.points.constFirst() - contour.points.constLast()).lengthSquared() <= gapSquared) {
                contour.points.removeLast();
                contour.closed = true;
            }
        }
        auto bestDistance = gapSquared;
        int bestFirst = -1;
        int bestSecond = -1;
        int bestJoin = 0;
        for (int first = 0; first < contours->size(); ++first) {
            const auto& a = contours->at(first);
            if (a.closed || a.points.size() < 2) continue;
            for (int second = first + 1; second < contours->size(); ++second) {
                const auto& b = contours->at(second);
                if (b.closed || b.points.size() < 2) continue;
                // 0: a.end-b.start, 1: a.end-b.end, 2: a.start-b.end, 3: a.start-b.start
                const std::array<float, 4> distances{
                    (a.points.constLast() - b.points.constFirst()).lengthSquared(),
                    (a.points.constLast() - b.points.constLast()).lengthSquared(),
                    (a.points.constFirst() - b.points.constLast()).lengthSquared(),
                    (a.points.constFirst() - b.points.constFirst()).lengthSquared()};
                for (int join = 0; join < 4; ++join) {
                    if (distances[static_cast<std::size_t>(join)] <= bestDistance) {
                        bestDistance = distances[static_cast<std::size_t>(join)];
                        bestFirst = first;
                        bestSecond = second;
                        bestJoin = join;
                    }
                }
            }
        }
        if (bestFirst < 0) break;
        auto& a = (*contours)[bestFirst];
        auto b = contours->at(bestSecond).points;
        if (bestJoin == 1 || bestJoin == 3) std::reverse(b.begin(), b.end());
        if (bestJoin <= 1) {
            a.points.append(b);
        } else {
            b.append(a.points);
            a.points = std::move(b);
        }
        contours->removeAt(bestSecond);
        merged = true;
    }
}

// A cap is the region inside the cut: every closed contour bounds it, and contours nest --
// a tube's bore is a hole in its wall, a boss inside that bore is material again. Each
// contour's depth (how many others enclose it) says which: even is material, odd is a hole
// of the contour that directly encloses it. Holes are bridged into their outer contour so
// one ear-clipping pass fills the region between them (D. Eberly, "Triangulation by Ear
// Clipping"); clipping each contour on its own, as before, filled the bore over.
class CapTriangulator final {
public:
    CapTriangulator(SectionBuildResult* result, const QVector3D& horizontal, const QVector3D& vertical)
        : result_(result), horizontal_(horizontal), vertical_(vertical) {}

    void fill(const QVector<Contour>& contours)
    {
        struct Ring final {
            int first{};
            int count{};
            double area{};
            double minX{}, minY{}, maxX{}, maxY{};
            int parent{-1};
            int depth{};
        };
        QVector<Ring> rings;
        for (const auto& contour : contours) {
            if (!contour.closed || contour.points.size() < 3) continue;
            Ring ring;
            ring.first = static_cast<int>(points_.size());
            ring.count = static_cast<int>(contour.points.size());
            ring.minX = ring.minY = std::numeric_limits<double>::max();
            ring.maxX = ring.maxY = std::numeric_limits<double>::lowest();
            for (const auto& point : contour.points) {
                const auto x = static_cast<double>(QVector3D::dotProduct(point, horizontal_));
                const auto y = static_cast<double>(QVector3D::dotProduct(point, vertical_));
                points_.push_back({x, y, point});
                ring.minX = std::min(ring.minX, x);
                ring.minY = std::min(ring.minY, y);
                ring.maxX = std::max(ring.maxX, x);
                ring.maxY = std::max(ring.maxY, y);
            }
            ring.area = signedArea(ring.first, ring.count);
            const auto span = std::max(ring.maxX - ring.minX, ring.maxY - ring.minY);
            if (std::abs(ring.area) <= span * span * 1.0e-12) continue;
            rings.append(ring);
        }
        if (rings.isEmpty()) return;

        // Largest first, so a contour's possible parents are all before it.
        QVector<int> order(rings.size());
        for (int index = 0; index < rings.size(); ++index) order[index] = index;
        std::sort(order.begin(), order.end(), [&rings](const int left, const int right) {
            return std::abs(rings.at(left).area) > std::abs(rings.at(right).area);
        });
        for (int position = 0; position < order.size(); ++position) {
            auto& ring = rings[order.at(position)];
            for (int candidatePosition = position - 1; candidatePosition >= 0; --candidatePosition) {
                const auto& candidate = rings.at(order.at(candidatePosition));
                if (ring.minX < candidate.minX || ring.maxX > candidate.maxX
                    || ring.minY < candidate.minY || ring.maxY > candidate.maxY)
                    continue;
                if (!ringContains(candidate.first, candidate.count, ring.first, ring.count)) continue;
                // The smallest enclosing ring is the nearest before it in the order.
                ring.parent = order.at(candidatePosition);
                ring.depth = candidate.depth + 1;
                break;
            }
        }

        for (int index = 0; index < rings.size(); ++index) {
            const auto& outer = rings.at(index);
            if (outer.depth % 2 != 0) continue;
            auto start = buildRing(outer.first, outer.count, true);
            if (start < 0) continue;
            QVector<int> holeStarts;
            for (int holeIndex = 0; holeIndex < rings.size(); ++holeIndex) {
                const auto& hole = rings.at(holeIndex);
                if (hole.parent != index || hole.depth % 2 == 0) continue;
                const auto holeStart = buildRing(hole.first, hole.count, false);
                if (holeStart >= 0) holeStarts.append(rightmost(holeStart));
            }
            // Rightmost hole first: its bridge then runs to the outer contour, or to a hole
            // already joined to it, never through a hole still waiting.
            std::sort(holeStarts.begin(), holeStarts.end(), [this](const int left, const int right) {
                return x(left) > x(right);
            });
            for (const auto hole : holeStarts) {
                const auto bridge = findBridge(hole, start);
                if (bridge < 0) continue;
                splice(bridge, hole);
            }
            clip(start);
        }
    }

private:
    struct Point2 final {
        double x{};
        double y{};
        QVector3D source;
    };
    struct Node final {
        int point{};
        int previous{};
        int next{};
        bool removed{};
    };

    [[nodiscard]] double x(const int node) const { return points_[static_cast<std::size_t>(nodes_[static_cast<std::size_t>(node)].point)].x; }
    [[nodiscard]] double y(const int node) const { return points_[static_cast<std::size_t>(nodes_[static_cast<std::size_t>(node)].point)].y; }
    [[nodiscard]] Node& node(const int index) { return nodes_[static_cast<std::size_t>(index)]; }
    [[nodiscard]] const Node& node(const int index) const { return nodes_[static_cast<std::size_t>(index)]; }

    [[nodiscard]] double signedArea(const int first, const int count) const
    {
        double area{};
        for (int index = 0; index < count; ++index) {
            const auto& a = points_[static_cast<std::size_t>(first + index)];
            const auto& b = points_[static_cast<std::size_t>(first + (index + 1) % count)];
            area += a.x * b.y - b.x * a.y;
        }
        return area * 0.5;
    }

    [[nodiscard]] bool pointInRing(const double px, const double py, const int first, const int count) const
    {
        bool inside = false;
        for (int index = 0, previous = count - 1; index < count; previous = index++) {
            const auto& a = points_[static_cast<std::size_t>(first + index)];
            const auto& b = points_[static_cast<std::size_t>(first + previous)];
            if ((a.y > py) != (b.y > py) && px < (b.x - a.x) * (py - a.y) / (b.y - a.y) + a.x)
                inside = !inside;
        }
        return inside;
    }

    // Contours of one cut do not cross, but they can touch: a vertex of the inner one may lie
    // on the outer one. A majority of a few sampled vertices settles it either way.
    [[nodiscard]] bool ringContains(const int outerFirst, const int outerCount, const int first, const int count) const
    {
        const auto samples = std::min(count, 5);
        int inside = 0;
        for (int sample = 0; sample < samples; ++sample) {
            const auto& point = points_[static_cast<std::size_t>(first + sample * count / samples)];
            if (pointInRing(point.x, point.y, outerFirst, outerCount)) ++inside;
        }
        return inside * 2 > samples;
    }

    // A ring as a circular list, wound counter-clockwise for material and clockwise for a hole.
    int buildRing(const int first, const int count, const bool counterClockwise)
    {
        const auto reverse = (signedArea(first, count) > 0.0) != counterClockwise;
        const auto start = static_cast<int>(nodes_.size());
        for (int index = 0; index < count; ++index) {
            const auto point = reverse ? first + count - 1 - index : first + index;
            nodes_.push_back({point, start + (index + count - 1) % count, start + (index + 1) % count, false});
        }
        return count >= 3 ? start : -1;
    }

    [[nodiscard]] int rightmost(const int start) const
    {
        auto best = start;
        auto current = node(start).next;
        while (current != start) {
            if (x(current) > x(best) || (x(current) == x(best) && y(current) < y(best))) best = current;
            current = node(current).next;
        }
        return best;
    }

    [[nodiscard]] double cross(const int a, const int b, const int c) const
    {
        return (x(b) - x(a)) * (y(c) - y(a)) - (y(b) - y(a)) * (x(c) - x(a));
    }

    [[nodiscard]] static bool inTriangle(const double ax, const double ay, const double bx, const double by,
                                         const double cx, const double cy, const double px, const double py)
    {
        const auto d1 = (bx - ax) * (py - ay) - (by - ay) * (px - ax);
        const auto d2 = (cx - bx) * (py - by) - (cy - by) * (px - bx);
        const auto d3 = (ax - cx) * (py - cy) - (ay - cy) * (px - cx);
        const auto negative = d1 < 0.0 || d2 < 0.0 || d3 < 0.0;
        const auto positive = d1 > 0.0 || d2 > 0.0 || d3 > 0.0;
        return !(negative && positive);
    }

    // The outer-ring vertex the hole's rightmost vertex can see: cast a ray to +x, take the
    // nearest edge it hits, and that edge's far end -- unless a reflex vertex sits inside the
    // triangle between, in which case the one closest in angle to the ray.
    [[nodiscard]] int findBridge(const int hole, const int outer) const
    {
        const auto hx = x(hole);
        const auto hy = y(hole);
        auto nearest = std::numeric_limits<double>::max();
        int candidate = -1;
        auto current = outer;
        do {
            const auto next = node(current).next;
            const auto ay = y(current);
            const auto by = y(next);
            // Edges running upward have the material to their left, facing the hole.
            if (ay <= hy && by > hy) {
                const auto ix = x(current) + (hy - ay) * (x(next) - x(current)) / (by - ay);
                if (ix >= hx && ix < nearest) {
                    nearest = ix;
                    candidate = ay == hy ? current : (x(current) > x(next) ? current : next);
                }
            }
            current = next;
        } while (current != outer);
        if (candidate < 0) return -1;
        if (x(candidate) == nearest && y(candidate) == hy) return candidate;

        auto best = candidate;
        auto bestTangent = std::numeric_limits<double>::max();
        auto bestDistance = std::numeric_limits<double>::max();
        current = outer;
        do {
            if (current != candidate && x(current) > hx
                && inTriangle(hx, hy, nearest, hy, x(candidate), y(candidate), x(current), y(current))
                && cross(node(current).previous, current, node(current).next) <= 0.0) {
                const auto tangent = std::abs(y(current) - hy) / (x(current) - hx);
                const auto distance = (x(current) - hx) * (x(current) - hx) + (y(current) - hy) * (y(current) - hy);
                if (tangent < bestTangent || (tangent == bestTangent && distance < bestDistance)) {
                    best = current;
                    bestTangent = tangent;
                    bestDistance = distance;
                }
            }
            current = node(current).next;
        } while (current != outer);
        return best;
    }

    // outer -> hole ... around the hole ... -> hole' -> outer' -> on round the outer ring.
    void splice(const int outer, const int hole)
    {
        const auto outerCopy = static_cast<int>(nodes_.size());
        nodes_.push_back(node(outer));
        const auto holeCopy = static_cast<int>(nodes_.size());
        nodes_.push_back(node(hole));
        const auto outerNext = node(outer).next;
        const auto holePrevious = node(hole).previous;
        node(outer).next = hole;
        node(hole).previous = outer;
        node(outerCopy).next = outerNext;
        node(outerNext).previous = outerCopy;
        node(holeCopy).next = outerCopy;
        node(outerCopy).previous = holeCopy;
        node(holePrevious).next = holeCopy;
        node(holeCopy).previous = holePrevious;
    }

    [[nodiscard]] bool samePosition(const int a, const int b) const { return x(a) == x(b) && y(a) == y(b); }

    [[nodiscard]] bool isEar(const int ear) const
    {
        const auto a = node(ear).previous;
        const auto c = node(ear).next;
        if (cross(a, ear, c) <= 0.0) return false;
        const auto minX = std::min({x(a), x(ear), x(c)});
        const auto maxX = std::max({x(a), x(ear), x(c)});
        const auto minY = std::min({y(a), y(ear), y(c)});
        const auto maxY = std::max({y(a), y(ear), y(c)});
        auto current = node(c).next;
        while (current != a) {
            const auto px = x(current);
            const auto py = y(current);
            if (px >= minX && px <= maxX && py >= minY && py <= maxY
                && !samePosition(current, a) && !samePosition(current, ear) && !samePosition(current, c)
                && inTriangle(x(a), y(a), x(ear), y(ear), x(c), y(c), px, py)
                && cross(node(current).previous, current, node(current).next) <= 0.0)
                return false;
            current = node(current).next;
        }
        return true;
    }

    void emitTriangle(const int a, const int b, const int c)
    {
        const auto append = [this](const int index) {
            const auto& point = points_[static_cast<std::size_t>(node(index).point)].source;
            const auto vertex = static_cast<quint32>(result_->vertices.size() / 3);
            result_->vertices.append(point.x());
            result_->vertices.append(point.y());
            result_->vertices.append(point.z());
            result_->indices.append(vertex);
        };
        append(a); append(b); append(c);
        append(c); append(b); append(a);
    }

    int unlink(const int index)
    {
        const auto previous = node(index).previous;
        const auto next = node(index).next;
        node(previous).next = next;
        node(next).previous = previous;
        node(index).removed = true;
        return next;
    }

    // Drops vertices that add no area: repeats, and points on a straight run.
    int dropDegenerate(int start, int* remaining)
    {
        auto current = start;
        auto checked = 0;
        while (*remaining > 3 && checked < *remaining) {
            const auto previous = node(current).previous;
            const auto next = node(current).next;
            if (samePosition(current, next) || cross(previous, current, next) == 0.0) {
                if (current == start) start = next;
                current = unlink(current);
                --*remaining;
                checked = 0;
            } else {
                current = next;
                ++checked;
            }
        }
        return start;
    }

    // Ear clipping that always finishes: when no clean ear is left (the contour touches
    // itself, or rounding made a sliver), degenerate vertices go first and then the most
    // convex corner is cut regardless, so the cap is complete rather than abandoned half-built.
    void clip(int start)
    {
        auto remaining = 1;
        for (auto current = node(start).next; current != start; current = node(current).next) ++remaining;
        auto current = start;
        auto sinceLastEar = 0;
        while (remaining > 3) {
            if (isEar(current)) {
                emitTriangle(node(current).previous, current, node(current).next);
                current = node(unlink(current)).previous;
                --remaining;
                sinceLastEar = 0;
                continue;
            }
            current = node(current).next;
            if (++sinceLastEar < remaining) continue;
            current = dropDegenerate(current, &remaining);
            if (remaining <= 3) break;
            auto forced = -1;
            auto forcedArea = 0.0;
            auto probe = current;
            for (int index = 0; index < remaining; ++index, probe = node(probe).next) {
                const auto area = cross(node(probe).previous, probe, node(probe).next);
                if (area > forcedArea) {
                    forcedArea = area;
                    forced = probe;
                }
            }
            if (forced < 0) return;
            emitTriangle(node(forced).previous, forced, node(forced).next);
            current = node(unlink(forced)).previous;
            --remaining;
            sinceLastEar = 0;
        }
        if (remaining == 3 && cross(node(current).previous, current, node(current).next) > 0.0)
            emitTriangle(node(current).previous, current, node(current).next);
    }

    SectionBuildResult* result_;
    QVector3D horizontal_;
    QVector3D vertical_;
    std::vector<Point2> points_;
    std::vector<Node> nodes_;
};

void appendRoundedOutline(SectionBuildResult* result, const QVector<Contour>& contours,
                          const QVector3D& normal, const float halfWidth, const QVector3D& depthLift)
{
    if (!result || halfWidth <= 0.0F) return;
    const auto reference = std::abs(normal.z()) < 0.9F ? QVector3D{0.0F, 0.0F, 1.0F}
                                                        : QVector3D{0.0F, 1.0F, 0.0F};
    const auto horizontal = QVector3D::crossProduct(reference, normal).normalized();
    const auto vertical = QVector3D::crossProduct(normal, horizontal).normalized();
    const auto appendVertex = [result](const QVector3D& point) {
        const auto index = static_cast<quint32>(result->vertices.size() / 3);
        result->vertices.append(point.x());
        result->vertices.append(point.y());
        result->vertices.append(point.z());
        result->indices.append(index);
    };
    const auto appendDoubleSidedTriangle = [&appendVertex](const QVector3D& first, const QVector3D& second,
                                                           const QVector3D& third) {
        appendVertex(first); appendVertex(second); appendVertex(third);
        appendVertex(third); appendVertex(second); appendVertex(first);
    };
    const auto appendDisk = [&appendDoubleSidedTriangle, &horizontal, &vertical, halfWidth, &depthLift](const QVector3D& center) {
        constexpr int segments = 10;
        constexpr float pi = 3.14159265358979323846F;
        const auto liftedCenter = center + depthLift;
        for (int index = 0; index < segments; ++index) {
            const auto firstAngle = static_cast<float>(index) * 2.0F * pi / segments;
            const auto secondAngle = static_cast<float>(index + 1) * 2.0F * pi / segments;
            const auto first = liftedCenter + horizontal * (std::cos(firstAngle) * halfWidth)
                    + vertical * (std::sin(firstAngle) * halfWidth);
            const auto second = liftedCenter + horizontal * (std::cos(secondAngle) * halfWidth)
                    + vertical * (std::sin(secondAngle) * halfWidth);
            appendDoubleSidedTriangle(liftedCenter, first, second);
        }
    };

    for (const auto& contour : contours) {
        if (contour.points.size() < 2) continue;
        const auto segmentCount = contour.closed ? contour.points.size() : contour.points.size() - 1;
        for (qsizetype index = 0; index < segmentCount; ++index) {
            const auto& first = contour.points.at(index);
            const auto& second = contour.points.at((index + 1) % contour.points.size());
            const auto direction = (second - first).normalized();
            const auto offset = QVector3D::crossProduct(normal, direction).normalized() * halfWidth;
            if (offset.isNull()) continue;
            const auto firstLeft = first - offset + depthLift;
            const auto firstRight = first + offset + depthLift;
            const auto secondLeft = second - offset + depthLift;
            const auto secondRight = second + offset + depthLift;
            appendDoubleSidedTriangle(firstLeft, secondLeft, secondRight);
            appendDoubleSidedTriangle(firstLeft, secondRight, firstRight);
        }
        for (const auto& point : contour.points) appendDisk(point);
    }
}

} // namespace

bool sectionPlaneCrossesBox(const QVector3D& minimum, const QVector3D& maximum, const QVector3D& normal,
                            const double offset)
{
    if (normal.isNull() || minimum.x() > maximum.x()) return false;
    const auto unit = normal.normalized();
    const QVector3D centre = (minimum + maximum) * 0.5F;
    const QVector3D half = (maximum - minimum) * 0.5F;
    // The box's reach along the normal, either side of its centre, plus a hair so a face
    // lying exactly in the plane still counts as cut.
    const auto reach = static_cast<double>(std::abs(unit.x()) * half.x() + std::abs(unit.y()) * half.y()
                                           + std::abs(unit.z()) * half.z());
    const auto distance = static_cast<double>(QVector3D::dotProduct(unit, centre)) - offset;
    return std::abs(distance) <= reach + std::max(1.0e-5, reach * 1.0e-6);
}

SectionBuildResult buildSectionOverlay(const SectionBuildRequest& request)
{
    SectionBuildResult result;
    if (!request.source || request.source->vertices.isEmpty() || request.source->indices.isEmpty()
        || request.normal.isNull())
        return result;

    const auto& sourceVertices = request.source->vertices;
    const auto& sourceIndices = request.source->indices;
    const auto normal = request.normal.normalized();
    const auto diagonal = sourceDiagonal(sourceVertices);
    const auto tolerance = std::max(0.00001F, diagonal * 0.000001F);
    const auto toleranceSquared = tolerance * tolerance;
    const auto pointAt = [&sourceVertices](const quint32 index) {
        const auto offset = static_cast<qsizetype>(index) * 3;
        return QVector3D(sourceVertices.at(offset), sourceVertices.at(offset + 1), sourceVertices.at(offset + 2));
    };
    const auto distance = [&request, &normal](const QVector3D& point) {
        return static_cast<double>(QVector3D::dotProduct(normal, point)) - request.offset;
    };
    const auto inside = [&request](const double value) { return request.flipped ? value <= 0.0 : value >= 0.0; };

    // Each vertex's distance to the plane, once: shared vertices were measured again for
    // every triangle and edge they belong to.
    const auto vertexCount = sourceVertices.size() / 3;
    std::vector<double> distances(static_cast<std::size_t>(vertexCount));
    for (qsizetype vertex = 0; vertex < vertexCount; ++vertex)
        distances[static_cast<std::size_t>(vertex)] = distance(pointAt(static_cast<quint32>(vertex)));

    QVector<Segment> segments;
    for (qsizetype triangle = 0; triangle + 2 < sourceIndices.size(); triangle += 3) {
        const std::array<quint32, 3> corner{sourceIndices.at(triangle), sourceIndices.at(triangle + 1),
                                            sourceIndices.at(triangle + 2)};
        const std::array<double, 3> cornerDistance{distances[corner[0]], distances[corner[1]], distances[corner[2]]};
        // A triangle wholly on one side has nothing to add: the common case, skipped
        // before any point is built.
        if (inside(cornerDistance[0]) == inside(cornerDistance[1]) && inside(cornerDistance[1]) == inside(cornerDistance[2]))
            continue;
        std::array<QVector3D, 2> intersections;
        int found = 0;
        for (int index = 0; index < 3 && found < 2; ++index) {
            const auto next = (index + 1) % 3;
            const auto startDistance = cornerDistance[index];
            const auto endDistance = cornerDistance[next];
            if (inside(startDistance) == inside(endDistance)) continue;
            const auto start = pointAt(corner[index]);
            const auto end = pointAt(corner[next]);
            const auto amount = static_cast<float>(startDistance / (startDistance - endDistance));
            const auto intersection = start + (end - start) * amount;
            if (found == 0 || (intersections[found - 1] - intersection).lengthSquared() > toleranceSquared)
                intersections[found++] = intersection;
        }
        if (found == 2 && (intersections[0] - intersections[1]).lengthSquared() > toleranceSquared)
            segments.append({intersections[0], intersections[1]});
    }

    auto contours = assembleContours(segments, tolerance);
    stitchOpenContours(&contours, std::max(tolerance * 4.0F, diagonal * 0.0005F));
    for (auto& contour : contours) simplifyContour(&contour.points, normal, tolerance, contour.closed);

    if (request.sliceOnly && !request.sliceFill) {
        if (request.sliceOutline) {
            const auto halfWidth = request.outlineWidth > 0.0F
                    ? request.outlineWidth * 0.5F : std::max(0.01F, diagonal * 0.00035F);
            const auto removedNormal = normal * (request.flipped ? 1.0F : -1.0F);
            appendRoundedOutline(&result, contours, normal, halfWidth,
                                 removedNormal * std::max(diagonal * 0.000002F, halfWidth * 0.025F));
        }
        result.capTriangleCount = static_cast<int>(result.indices.size() / 3);
        return result;
    }

    if ((request.capEnabled || (request.sliceOnly && request.sliceFill)) && !contours.isEmpty()) {
        const auto reference = std::abs(normal.z()) < 0.9F ? QVector3D{0.0F, 0.0F, 1.0F}
                                                            : QVector3D{0.0F, 1.0F, 0.0F};
        const auto horizontal = QVector3D::crossProduct(reference, normal).normalized();
        const auto vertical = QVector3D::crossProduct(normal, horizontal).normalized();
        CapTriangulator(&result, horizontal, vertical).fill(contours);
    }
    result.fillIndexCount = static_cast<int>(result.indices.size());

    if (request.sliceOnly && request.sliceOutline) {
        const auto halfWidth = request.outlineWidth > 0.0F
                ? request.outlineWidth * 0.5F : std::max(0.01F, diagonal * 0.00035F);
        const auto removedNormal = normal * (request.flipped ? 1.0F : -1.0F);
        appendRoundedOutline(&result, contours, normal, halfWidth,
                             removedNormal * std::max(diagonal * 0.000002F, halfWidth * 0.025F));
    }
    result.capTriangleCount = request.sliceOnly ? static_cast<int>(result.indices.size() / 3)
                                                : result.fillIndexCount / 3;
    return result;
}

} // namespace loupe::app::render
