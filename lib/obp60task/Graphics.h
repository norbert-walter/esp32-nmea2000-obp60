#pragma once
#include <vector>

struct Point {
    double x;
    double y;
};

struct Rect {
    double x;
    double y;
    double w;
    double h;
};

Point rotatePoint(const Point& origin, const Point& p, double angle);
std::vector<Point> rotatePoints(const Point& origin, const std::vector<Point>& pts, double angle);
