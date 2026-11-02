#pragma once
#import <UIKit/UIKit.h>

#include "mobile_app.hpp"

// Геометрия окна и выреза меняется независимо: поворот на 180° сохраняет размер кадра.
struct IosViewLayout {
    uint32_t width = 0, height = 0;
    mobile::SafeArea safe;
    UIInterfaceOrientation orientation = UIInterfaceOrientationUnknown;

    bool update(UIView* view);
};
