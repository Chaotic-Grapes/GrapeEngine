/**
* @Name: Daniel (to be added_
* @email: to be added
* @file AStarPathfinder.cs
* @brief Grid-based A* pathfinding with obstacle cache, LOS smoothing, and helpers.
*
* @details
* Provides a static pathfinding service used by EnemyAI:
* - World<->grid conversions using fixed cell size
* - Obstacle initialization and cached list access
* - A* search over 8-connected grid with octile heuristic
* - Path post-processing via visibility (line-of-sight) smoothing
* - Utility checks (walkability, nearest-walkable, LOS samples)
*
* All math uses GrapeEngine.Numerics vectors; logic is kept deterministic by
* avoiding randomness in the core search. Path smoothing samples along segments
* to allow corner cutting while respecting a small margin from obstacles.
*
* @sources
*  to be added
*
* @dependencies
* - using GrapeEngine.Numerics
* - System.Collections.Generic, System
*/
using GrapeEngine.Numerics;
using System.Collections.Generic;
using System;

namespace MyGame
{
    public static class AStarPathfinder
    {
        private static List<Vector3> m_obstacles = new List<Vector3>();

        // grid
        private const float CELL_SIZE = 5.0f;
        private const int GRID_WIDTH = 320;
        private const int GRID_HEIGHT = 180;

        // obstacle dimensions
        private const float OBSTACLE_WIDTH = 20.0f;
        private const float OBSTACLE_HEIGHT = 20.0f;

        private static bool m_initialized = false;

        static AStarPathfinder()
        {
            InitializeObstacles();
            m_initialized = true;
        }

        public static void EnsureInitialized()
        {
            if (!m_initialized)
            {
                InitializeObstacles();
                m_initialized = true;
            }
        }

        private static void InitializeObstacles()
        {
            m_obstacles.Clear();

            float spacing = 20.0f;

            // test 1: left vertical wall
            for (float y = 200f; y < 500f; y += spacing)
            {
                m_obstacles.Add(new Vector3(200f, y, 0f));
            }

            // test 2: horizontal walls
            for (float x = 300f; x < 600f; x += spacing)
            {
                m_obstacles.Add(new Vector3(x, 550f, 0f));
            }
            for (float x = 800f; x < 1100f; x += spacing)
            {
                m_obstacles.Add(new Vector3(x, 550f, 0f));
            }

            // test 3: L shaped wall
            for (float y = 150f; y < 350f; y += spacing)
            {
                m_obstacles.Add(new Vector3(1200f, y, 0f));
            }
            for (float x = 1200f; x < 1500f; x += spacing)
            {
                m_obstacles.Add(new Vector3(x, 350f, 0f));
            }

            System.Console.WriteLine($"[A*] Initialized {m_obstacles.Count} obstacles");
        }

        public static List<Vector3> GetObstacles()
        {
            EnsureInitialized();
            return m_obstacles;
        }

        public static (float width, float height) GetObstacleDimensions()
        {
            return (OBSTACLE_WIDTH, OBSTACLE_HEIGHT);
        }

        public static List<Vector3> FindPath(Vector3 start, Vector3 target)
        {
            EnsureInitialized();

            Vector2Int startGrid = WorldToGrid(start);
            Vector2Int targetGrid = WorldToGrid(target);

            // debug
            // System.Console.WriteLine($"[A*] Finding path from {start} to {target}");
            // System.Console.WriteLine($"[A*] Grid coords: {startGrid} to {targetGrid}");

            if (!IsWalkable(startGrid))
            {
                startGrid = FindNearestWalkable(startGrid);
                // System.Console.WriteLine($"[A*] Start unwalkable, moved to {startGrid}"); // debug
            }

            if (!IsWalkable(targetGrid))
            {
                targetGrid = FindNearestWalkable(targetGrid);
                // System.Console.WriteLine($"[A*] Target unwalkable, moved to {targetGrid}"); // debug
            }

            var path = FindPathAStar(startGrid, targetGrid);

            if (path != null && path.Count > 0)
            {
                var worldPath = new List<Vector3>();
                foreach (var gridPos in path)
                {
                    worldPath.Add(GridToWorld(gridPos));
                }

                var smoothedPath = SmoothPath(worldPath, start);

                // System.Console.WriteLine($"[A*] Path found: {path.Count} grid points -> {smoothedPath.Count} smoothed waypoints"); // debug

                while (smoothedPath.Count > 1 && (smoothedPath[0] - start).Magnitude < 15f)
                {
                    smoothedPath.RemoveAt(0);
                }

                return smoothedPath;
            }

            System.Console.WriteLine("[A*] ERROR: No path found!");
            return new List<Vector3> { target };
        }

        private static List<Vector3> SmoothPath(List<Vector3> path, Vector3 actualStart)
        {
            if (path == null || path.Count <= 2)
                return path;

            var smoothed = new List<Vector3> { actualStart };
            int currentIndex = 0;

            while (currentIndex < path.Count - 1)
            {
                int farthestVisible = currentIndex + 1;

                for (int i = path.Count - 1; i > currentIndex + 1; i--)
                {
                    if (HasLineOfSight(currentIndex == 0 ? actualStart : path[currentIndex], path[i]))
                    {
                        farthestVisible = i;
                        break;
                    }
                }

                if (currentIndex == 0)
                {
                    smoothed.Add(path[farthestVisible]);
                }
                else
                {
                    smoothed.Add(path[farthestVisible]);
                }

                currentIndex = farthestVisible;
            }

            return smoothed;
        }

        // does Enemy have LOS?
        private static bool HasLineOfSight(Vector3 start, Vector3 end)
        {
            Vector3 direction = end - start;
            float distance = direction.Magnitude;

            if (distance < 0.1f)
                return true;

            direction = direction.Normalized;

            // check at small intervals
            int samples = Math.Max((int)(distance / 3.0f), 3);

            for (int i = 0; i <= samples; i++)
            {
                float t = i / (float)samples;
                Vector3 samplePoint = start + direction * (distance * t);

                // check if this point would collide
                if (WouldCollideAtPoint(samplePoint))
                {
                    return false;
                }
            }

            return true;
        }

        // direct collision check for a point (used by LOS)
        private static bool WouldCollideAtPoint(Vector3 point)
        {
            // buffer for LOS to allow corner cutting
            float halfWidth = OBSTACLE_WIDTH / 2f + 8.0f;
            float halfHeight = OBSTACLE_HEIGHT / 2f + 8.0f;

            foreach (var obstacle in m_obstacles)
            {
                if (Math.Abs(point.X - obstacle.X) < halfWidth &&
                    Math.Abs(point.Y - obstacle.Y) < halfHeight)
                {
                    return true;
                }
            }
            return false;
        }

        private static Vector2Int FindNearestWalkable(Vector2Int gridPos)
        {
            if (IsWalkable(gridPos)) return gridPos;

            // expanding search in a smarter order to find and choose closer points
            List<Vector2Int> candidates = new List<Vector2Int>();

            for (int radius = 1; radius <= 20; radius++)
            {
                for (int x = -radius; x <= radius; x++)
                {
                    for (int y = -radius; y <= radius; y++)
                    {
                        if (Math.Abs(x) == radius || Math.Abs(y) == radius)
                        {
                            Vector2Int neighbor = new Vector2Int(gridPos.X + x, gridPos.Y + y);
                            if (neighbor.X >= 0 && neighbor.X < GRID_WIDTH &&
                                neighbor.Y >= 0 && neighbor.Y < GRID_HEIGHT &&
                                IsWalkable(neighbor))
                            {
                                return neighbor;
                            }
                        }
                    }
                }
            }
            return gridPos;
        }

        private static List<Vector2Int> FindPathAStar(Vector2Int start, Vector2Int target)
        {
            var openSet = new SortedSet<PathNode>(Comparer<PathNode>.Create((a, b) =>
            {
                int cmp = a.F.CompareTo(b.F);
                if (cmp == 0) return a.Position.GetHashCode().CompareTo(b.Position.GetHashCode());
                return cmp;
            }));

            var closedSet = new HashSet<Vector2Int>();
            var cameFrom = new Dictionary<Vector2Int, Vector2Int>();
            var gScore = new Dictionary<Vector2Int, float>();

            gScore[start] = 0;
            openSet.Add(new PathNode { Position = start, F = Heuristic(start, target) });

            int iterations = 0;
            const int MAX_ITERATIONS = 3000;

            while (openSet.Count > 0 && iterations < MAX_ITERATIONS)
            {
                iterations++;

                var current = openSet.Min;
                openSet.Remove(current);

                if (current.Position == target)
                {
                    return ReconstructPath(cameFrom, current.Position);
                }

                closedSet.Add(current.Position);

                // all 8 directions
                int[,] directions = {
                    { 0, 1 }, { 1, 0 }, { 0, -1 }, { -1, 0 },
                    { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 }
                };

                for (int i = 0; i < directions.GetLength(0); i++)
                {
                    Vector2Int neighbor = new Vector2Int(
                        current.Position.X + directions[i, 0],
                        current.Position.Y + directions[i, 1]
                    );

                    if (neighbor.X < 0 || neighbor.X >= GRID_WIDTH ||
                        neighbor.Y < 0 || neighbor.Y >= GRID_HEIGHT)
                        continue;

                    if (closedSet.Contains(neighbor) || !IsWalkable(neighbor))
                        continue;

                    bool isDiagonal = (directions[i, 0] != 0 && directions[i, 1] != 0);
                    float moveCost = isDiagonal ? 1.414f : 1.0f;
                    float tentativeG = gScore[current.Position] + moveCost;

                    if (!gScore.ContainsKey(neighbor) || tentativeG < gScore[neighbor])
                    {
                        cameFrom[neighbor] = current.Position;
                        gScore[neighbor] = tentativeG;
                        float f = tentativeG + Heuristic(neighbor, target);

                        openSet.Add(new PathNode { Position = neighbor, F = f });
                    }
                }
            }

            return null;
        }

        // octile distance heuristic - for 8d movement
        private static float Heuristic(Vector2Int a, Vector2Int b)
        {
            int dx = Math.Abs(a.X - b.X);
            int dy = Math.Abs(a.Y - b.Y);
            return Math.Max(dx, dy) + 0.414f * Math.Min(dx, dy);
        }

        private static List<Vector2Int> ReconstructPath(Dictionary<Vector2Int, Vector2Int> cameFrom, Vector2Int current)
        {
            var path = new List<Vector2Int> { current };

            while (cameFrom.ContainsKey(current))
            {
                current = cameFrom[current];
                path.Insert(0, current);
            }

            return path;
        }

        private static Vector2Int WorldToGrid(Vector3 worldPos)
        {
            return new Vector2Int(
                (int)(worldPos.X / CELL_SIZE),
                (int)(worldPos.Y / CELL_SIZE)
            );
        }

        private static Vector3 GridToWorld(Vector2Int gridPos)
        {
            return new Vector3(
                gridPos.X * CELL_SIZE + CELL_SIZE * 0.5f,
                gridPos.Y * CELL_SIZE + CELL_SIZE * 0.5f,
                0
            );
        }

        // walkability check for pathfinding grid
        private static bool IsWalkable(Vector2Int gridPos)
        {
            Vector3 worldPos = GridToWorld(gridPos);

            float halfWidth = OBSTACLE_WIDTH / 2f + 25.0f;
            float halfHeight = OBSTACLE_HEIGHT / 2f + 25.0f;

            foreach (var obstacle in m_obstacles)
            {
                if (Math.Abs(worldPos.X - obstacle.X) < halfWidth &&
                    Math.Abs(worldPos.Y - obstacle.Y) < halfHeight)
                {
                    return false;
                }
            }

            return true;
        }

        private class PathNode
        {
            public Vector2Int Position;
            public float F;
        }
    }

    public struct Vector2Int
    {
        public int X, Y;

        public Vector2Int(int x, int y)
        {
            X = x;
            Y = y;
        }

        public static bool operator ==(Vector2Int a, Vector2Int b) => a.X == b.X && a.Y == b.Y;
        public static bool operator !=(Vector2Int a, Vector2Int b) => !(a == b);

        public override bool Equals(object obj) => obj is Vector2Int other && this == other;
        public override int GetHashCode() => (X, Y).GetHashCode();

        public override string ToString() => $"({X}, {Y})";
    }
}