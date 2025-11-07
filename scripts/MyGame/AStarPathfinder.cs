/* Start Header *****************************************************************/
/*!
\file    AStarPathfinder.cs
\author  Daniel Kay Neo Zuo Feng (100%)
\par     k.danielneozuofeng@digipen.edu
\brief
This file contains the A* pathfinding system for enemy AI navigation around obstacles.
Uses a grid-based approach with 8-direction movement and path smoothing for natural-looking
movement. The system handles rectangular obstacles and includes corner detection to prevent
enemies from getting stuck.

The pathfinder converts world positions to a grid, finds the shortest path using A* algorithm,
then smooths the path by removing unnecessary waypoints where line-of-sight exists. This
balances performance with smooth enemy movement.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

using GrapeEngine.Numerics;
using System.Collections.Generic;
using System;

namespace MyGame;

public static class AStarPathfinder
{
    private static List<Vector3> m_obstacles = new List<Vector3>();

    // grid settings - smaller cells = more precise pathfinding but slower
    // window size is 1600 x 900
    private const float CELL_SIZE = 5.0f;
    private const int GRID_WIDTH = 320;     // 1600 / 5
    private const int GRID_HEIGHT = 180;    // 900 / 5

    // how big the obstacles are
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

        // Test 1: Left vertical wall
        for (float y = 200f; y < 500f; y += spacing)
        {
            m_obstacles.Add(new Vector3(200f, y, 0f));
        }

        // Test 2: Horizontal walls
        for (float x = 300f; x < 600f; x += spacing)
        {
            m_obstacles.Add(new Vector3(x, 550f, 0f));
        }
        for (float x = 800f; x < 1100f; x += spacing)
        {
            m_obstacles.Add(new Vector3(x, 550f, 0f));
        }

        // Test 3: L shaped wall
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

        // convert real world positions to grid coordinates
        Vector2Int startGrid = WorldToGrid(start);
        Vector2Int targetGrid = WorldToGrid(target);

        // for debugging
        // System.Console.WriteLine($"[A*] Finding path from {start} to {target}");
        // System.Console.WriteLine($"[A*] Grid coords: {startGrid} to {targetGrid}");

        // if we are starting inside a wall, find the nearest open spot
        if (!IsWalkable(startGrid))
        {
            startGrid = FindNearestWalkable(startGrid);
            // System.Console.WriteLine($"[A*] Start was in wall, moved to {startGrid}"); // debug
        }

        // same for target position
        if (!IsWalkable(targetGrid))
        {
            targetGrid = FindNearestWalkable(targetGrid);
            // System.Console.WriteLine($"[A*] Target was in wall, moved to {targetGrid}"); // debug
        }

        // do the actual A* search on the grid
        var path = FindPathAStar(startGrid, targetGrid);

        if (path != null && path.Count > 0)
        {
            // convert grid positions back to real world coordinates
            var worldPath = new List<Vector3>();
            foreach (var gridPos in path)
            {
                worldPath.Add(GridToWorld(gridPos));
            }

            // smooth out the path of the enemy
            var smoothedPath = SmoothPath(worldPath, start);

            // System.Console.WriteLine($"[A*] Path found: {path.Count} grid points -> {smoothedPath.Count} smoothed waypoints"); // debug

            // remove points that are too close to where we already are
            while (smoothedPath.Count > 1 && (smoothedPath[0] - start).Magnitude < 15f)
            {
                smoothedPath.RemoveAt(0);
            }

            return smoothedPath;
        }

        System.Console.WriteLine("[A*] ERROR: No path found!");
        return new List<Vector3> { target };
    }

    // makes the path smoother by cutting corners where we have clear LOS
    private static List<Vector3> SmoothPath(List<Vector3> path, Vector3 actualStart)
    {
        if (path == null)       return new List<Vector3>();
        if (path.Count <= 2)    return path;

        var smoothed = new List<Vector3> { actualStart };
        int currentIndex = 0;

        // look ahead and skip intermediate points if we can see further ones
        while (currentIndex < path.Count - 1)
        {
            int farthestVisible = currentIndex + 1;

            // check from farthest back to find the furthest point we can see directly
            for (int i = path.Count - 1; i > currentIndex + 1; i--)
            {
                if (HasLineOfSight(currentIndex == 0 ? actualStart : path[currentIndex], path[i]))
                {
                    farthestVisible = i;
                    break;
                }
            }

            // add the furthest visible point to our smoothed path
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

    // does Enemy have LOS? check if we can see from start to end without hitting walls
    private static bool HasLineOfSight(Vector3 start, Vector3 end)
    {
        Vector3 direction = end - start;
        float distance = direction.Magnitude;

        if (distance < 0.1f)    return true;

        direction = direction.Normalized;

        // check several points along the line between start and end
        int samples = Math.Max((int)(distance / 3.0f), 3);

        for (int i = 0; i <= samples; i++)
        {
            float t = i / (float)samples;
            Vector3 samplePoint = start + direction * (distance * t);

            // if any point along the line hits a wall, no line of sight
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

    // if we're in a wall, find the nearest open grid cell
    private static Vector2Int FindNearestWalkable(Vector2Int gridPos)
    {
        if (IsWalkable(gridPos)) return gridPos;

        // expanding search in a smarter order to find and choose closer points
        List<Vector2Int> candidates = new List<Vector2Int>();

        // search in expanding circles around the position
        for (int radius = 1; radius <= 20; radius++)
        {
            for (int x = -radius; x <= radius; x++)
            {
                for (int y = -radius; y <= radius; y++)
                {
                    // only check the outer ring of each search radius
                    if (Math.Abs(x) == radius || Math.Abs(y) == radius)
                    {
                        Vector2Int neighbor = new Vector2Int(gridPos.X + x, gridPos.Y + y);
                        if (neighbor.X >= 0 && neighbor.X < GRID_WIDTH &&
                            neighbor.Y >= 0 && neighbor.Y < GRID_HEIGHT &&
                            IsWalkable(neighbor))
                        {
                            return neighbor;    // returns a found walkable spot
                        }
                    }
                }
            }
        }
        return gridPos; // give up and return original
    }

    // A* pathfinding algorithm
    private static List<Vector2Int> FindPathAStar(Vector2Int start, Vector2Int target)
    {
        // nodes to explore, sorted by lowest cost
        var openSet = new SortedSet<PathNode>(Comparer<PathNode>.Create((a, b) =>
        {
            int cmp = a.F.CompareTo(b.F);
            if (cmp == 0) return a.Position.GetHashCode().CompareTo(b.Position.GetHashCode());
            return cmp;
        }));

        var closedSet = new HashSet<Vector2Int>();                  // already explored nodes
        var cameFrom = new Dictionary<Vector2Int, Vector2Int>();    // path reconstruction
        var gScore = new Dictionary<Vector2Int, float>();           // cost from start to each node

        gScore[start] = 0;
        openSet.Add(new PathNode { Position = start, F = Heuristic(start, target) });

        int iterations = 0;
        const int MAX_ITERATIONS = 3000;    // safety net to prevent infinite loops

        while (openSet.Count > 0 && iterations < MAX_ITERATIONS)
        {
            iterations++;

            // get the most optimal node
            var current = openSet.Min;
            if (current == null) break;
            openSet.Remove(current);

            // if found target
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

            // checking all 8 directions
            for (int i = 0; i < directions.GetLength(0); i++)
            {
                Vector2Int neighbor = new Vector2Int(
                    current.Position.X + directions[i, 0],
                    current.Position.Y + directions[i, 1]
                );

                // skip if not in area
                if (neighbor.X < 0 || neighbor.X >= GRID_WIDTH ||
                    neighbor.Y < 0 || neighbor.Y >= GRID_HEIGHT)
                    continue;

                // skip if already checked or blocked by wall
                if (closedSet.Contains(neighbor) || !IsWalkable(neighbor))
                    continue;

                // diagonal movement costs more
                bool isDiagonal = (directions[i, 0] != 0 && directions[i, 1] != 0);
                float moveCost = isDiagonal ? 1.414f : 1.0f;    // sqrt of 2, thsi is done using pythagoras theorem
                float tentativeG = gScore[current.Position] + moveCost;

                // if this is a better path to the neighbour, update it
                if (!gScore.ContainsKey(neighbor) || tentativeG < gScore[neighbor])
                {
                    cameFrom[neighbor] = current.Position;
                    gScore[neighbor] = tentativeG;
                    float f = tentativeG + Heuristic(neighbor, target);

                    openSet.Add(new PathNode { Position = neighbor, F = f });
                }
            }
        }

        return new List<Vector2Int>();    // if no paths is found - returns empty list
    }

    // estimate cost from A to B - octile distance (8d movement)
    private static float Heuristic(Vector2Int a, Vector2Int b)
    {
        int dx = Math.Abs(a.X - b.X);
        int dy = Math.Abs(a.Y - b.Y);
        return Math.Max(dx, dy) + 0.414f * Math.Min(dx, dy);
    }

    // backtrack from target to start to build the final path
    private static List<Vector2Int> ReconstructPath(Dictionary<Vector2Int, Vector2Int> cameFrom, Vector2Int current)
    {
        var path = new List<Vector2Int> { current };

        // follow the chain of where we came from
        while (cameFrom.ContainsKey(current))
        {
            current = cameFrom[current];
            path.Insert(0, current);    // add to front to main the order
        }

        return path;
    }

    // convert real world position to grid cell
    private static Vector2Int WorldToGrid(Vector3 worldPos)
    {
        return new Vector2Int(
            (int)(worldPos.X / CELL_SIZE),
            (int)(worldPos.Y / CELL_SIZE)
        );
    }

    // convert grid cell back to real world position (at the center)
    private static Vector3 GridToWorld(Vector2Int gridPos)
    {
        return new Vector3(
            gridPos.X * CELL_SIZE + CELL_SIZE * 0.5f,
            gridPos.Y * CELL_SIZE + CELL_SIZE * 0.5f,
            0
        );
    }

    // walkability check for pathfinding grid (also makes sure its not blocked by walls) 
    private static bool IsWalkable(Vector2Int gridPos)
    {
        Vector3 worldPos = GridToWorld(gridPos);

        // use larger collision area for pathfinding to keep enemies away from walls
        float halfWidth = OBSTACLE_WIDTH / 2f + 25.0f;      // change the 25.0 if want the gap between obstacles
        float halfHeight = OBSTACLE_HEIGHT / 2f + 25.0f;    // and the enemy to be smaller/bigger to not get stuck.

        foreach (var obstacle in m_obstacles)
        {
            if (Math.Abs(worldPos.X - obstacle.X) < halfWidth &&
                Math.Abs(worldPos.Y - obstacle.Y) < halfHeight)
            {
                return false;   // if the cell is blocked
            }
        }

        return true;    // if the cell is clear
    }

    // helper class for A*
    // holds position and total cost
    private class PathNode
    {
        public Vector2Int Position;
        public float F; // following the formula, F = G (cost from start) + H (estimated cost to target)
    }
}

// simple 2D integer vector for grid positions
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

    public override bool Equals(object? obj) => obj is Vector2Int other && this == other;
    public override int GetHashCode() => (X, Y).GetHashCode();

    public override string ToString() => $"({X}, {Y})";
}