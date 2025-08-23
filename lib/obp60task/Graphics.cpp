/*
Generic graphics functions

*/
#include <math.h>
#include "Graphics.h"

Point rotatePoint(const Point& origin, const Point& p, double angle) {
    // rotate poind around origin by degrees
    Point rotated;
    double phi = angle * M_PI / 180.0;
    double dx = p.x - origin.x;
    double dy = p.y - origin.y;
    rotated.x = origin.x + cos(phi) * dx - sin(phi) * dy;
    rotated.y = origin.y + sin(phi) * dx + cos(phi) * dy;
    return rotated;
}

std::vector<Point> rotatePoints(const Point& origin, const std::vector<Point>& pts, double angle) {
    std::vector<Point> rotatedPoints;
    for (const auto& p : pts) {
         rotatedPoints.push_back(rotatePoint(origin, p, angle));
    }
     return rotatedPoints;
}
