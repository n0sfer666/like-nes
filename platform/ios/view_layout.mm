#include "view_layout.hpp"

#import <QuartzCore/CAMetalLayer.h>

bool IosViewLayout::update(UIView* view) {
    const CGFloat scale = view.traitCollection.displayScale;
    const uint32_t w = static_cast<uint32_t>(view.bounds.size.width * scale);
    const uint32_t h = static_cast<uint32_t>(view.bounds.size.height * scale);
    const UIEdgeInsets in = view.safeAreaInsets;
    const mobile::SafeArea next{static_cast<float>(in.left), static_cast<float>(in.top),
        static_cast<float>(in.right), static_cast<float>(in.bottom), static_cast<float>(scale)};
    const UIInterfaceOrientation next_orientation = view.window.windowScene.interfaceOrientation;
    const bool changed = w != width || h != height || next_orientation != orientation ||
        next.left != safe.left || next.top != safe.top || next.right != safe.right ||
        next.bottom != safe.bottom || next.scale != safe.scale;
    CAMetalLayer* layer = static_cast<CAMetalLayer*>(view.layer);
    layer.contentsScale = scale;
    layer.drawableSize = CGSizeMake(w, h);
    width = w;
    height = h;
    safe = next;
    orientation = next_orientation;
    return changed;
}
