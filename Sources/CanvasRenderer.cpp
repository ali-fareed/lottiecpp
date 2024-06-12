#include <LottieCpp/CanvasRenderer.h>

#include <cmath>

namespace lottie {

namespace {

// Values below that are invisible
static constexpr float minVisibleAlpha = 0.5f / 255.0f;

// Sometimes calculating path bounds is slower than just allocating the whole canvas
static constexpr float minGlobalRectCalculationSize = 200.0f;

struct TransformedPath {
    BezierPath path;
    Transform2D transform;
    
    TransformedPath(BezierPath const &path_, Transform2D const &transform_) :
    path(path_),
    transform(transform_) {
    }
};

static CGRect collectPathBoundingBoxes(std::shared_ptr<RenderTreeNodeContentItem> item, size_t subItemLimit, Transform2D const &parentTransform, bool skipApplyTransform, BezierPathsBoundingBoxContext &bezierPathsBoundingBoxContext) {
    //TODO:remove skipApplyTransform
    Transform2D effectiveTransform = parentTransform;
    if (!skipApplyTransform && item->isGroup) {
        effectiveTransform = item->transform * effectiveTransform;
    }
    
    size_t maxSubitem = std::min(item->subItems.size(), subItemLimit);
    
    CGRect boundingBox(0.0, 0.0, 0.0, 0.0);
    if (item->trimmedPaths) {
        for (const auto &path : item->trimmedPaths.value()) {
            if (path->needsBoundsRecalculation) {
                path->bounds = bezierPathsBoundingBoxParallel(bezierPathsBoundingBoxContext, path->path);
                path->needsBoundsRecalculation = false;
            }
            CGRect subpathBoundingBox = path->bounds.applyingTransform(effectiveTransform);
            if (boundingBox.empty()) {
                boundingBox = subpathBoundingBox;
            } else {
                boundingBox = boundingBox.unionWith(subpathBoundingBox);
            }
        }
    } else {
        if (item->path) {
            if (item->path->needsBoundsRecalculation) {
                item->path->bounds = bezierPathsBoundingBoxParallel(bezierPathsBoundingBoxContext, item->path->path);
                item->path->needsBoundsRecalculation = false;
            }
            boundingBox = item->path->bounds.applyingTransform(effectiveTransform);
        }
        
        for (size_t i = 0; i < maxSubitem; i++) {
            auto &subItem = item->subItems[i];
            
            CGRect subItemBoundingBox = collectPathBoundingBoxes(subItem, INT32_MAX, effectiveTransform, false, bezierPathsBoundingBoxContext);
            
            if (boundingBox.empty()) {
                boundingBox = subItemBoundingBox;
            } else {
                boundingBox = boundingBox.unionWith(subItemBoundingBox);
            }
        }
    }
    
    return boundingBox;
}

static void enumeratePaths(std::shared_ptr<RenderTreeNodeContentItem> item, size_t subItemLimit, Transform2D const &parentTransform, bool skipApplyTransform, std::function<void(BezierPath const &path, Transform2D const &transform)> const &onPath) {
    //TODO:remove skipApplyTransform
    Transform2D effectiveTransform = parentTransform;
    if (!skipApplyTransform && item->isGroup) {
        effectiveTransform = item->transform * effectiveTransform;
    }
    
    size_t maxSubitem = std::min(item->subItems.size(), subItemLimit);
    
    if (item->trimmedPaths) {
        for (const auto &path : item->trimmedPaths.value()) {
            onPath(path->path, effectiveTransform);
        }
        
        return;
    }
    
    if (item->path) {
        onPath(item->path->path, effectiveTransform);
    }
    
    for (size_t i = 0; i < maxSubitem; i++) {
        auto &subItem = item->subItems[i];
        
        enumeratePaths(subItem, INT32_MAX, effectiveTransform, false, onPath);
    }
}

}

static std::optional<CGRect> getRenderContentItemGlobalRect(std::shared_ptr<RenderTreeNodeContentItem> const &contentItem, Vector2D const &globalSize, Transform2D const &parentTransform, BezierPathsBoundingBoxContext &bezierPathsBoundingBoxContext) {
    auto currentTransform = parentTransform;
    Transform2D localTransform = contentItem->transform;
    currentTransform = localTransform * currentTransform;
    
    std::optional<CGRect> globalRect;
    for (const auto &shadingVariant : contentItem->shadings) {
        CGRect shapeBounds = collectPathBoundingBoxes(contentItem, shadingVariant->subItemLimit, Transform2D::identity(), true, bezierPathsBoundingBoxContext);
        
        if (shadingVariant->stroke) {
            shapeBounds = shapeBounds.insetBy(-shadingVariant->stroke->lineWidth / 2.0, -shadingVariant->stroke->lineWidth / 2.0);
        } else if (shadingVariant->fill) {
        } else {
            continue;
        }
        
        CGRect shapeGlobalBounds = shapeBounds.applyingTransform(currentTransform);
        if (globalRect) {
            globalRect = globalRect->unionWith(shapeGlobalBounds);
        } else {
            globalRect = shapeGlobalBounds;
        }
    }
    
    for (const auto &subItem : contentItem->subItems) {
        auto subGlobalRect = getRenderContentItemGlobalRect(subItem, globalSize, currentTransform, bezierPathsBoundingBoxContext);
        if (subGlobalRect) {
            if (globalRect) {
                globalRect = globalRect->unionWith(subGlobalRect.value());
            } else {
                globalRect = subGlobalRect.value();
            }
        }
    }
    
    if (globalRect) {
        CGRect integralGlobalRect(
            std::floor(globalRect->x),
            std::floor(globalRect->y),
            std::ceil(globalRect->width + globalRect->x - floor(globalRect->x)),
            std::ceil(globalRect->height + globalRect->y - floor(globalRect->y))
        );
        return integralGlobalRect.intersection(CGRect(0.0, 0.0, globalSize.x, globalSize.y));
    } else {
        return std::nullopt;
    }
}

static std::optional<CGRect> getRenderNodeGlobalRect(std::shared_ptr<RenderTreeNode> const &node, Vector2D const &globalSize, Transform2D const &parentTransform, bool isInvertedMatte, BezierPathsBoundingBoxContext &bezierPathsBoundingBoxContext) {
    if (node->isHidden() || node->alpha() < minVisibleAlpha) {
        return std::nullopt;
    }
    
    auto currentTransform = parentTransform;
    Transform2D localTransform = node->transform();
    currentTransform = localTransform * currentTransform;
    
    std::optional<CGRect> globalRect;
    if (node->_contentItem) {
        globalRect = getRenderContentItemGlobalRect(node->_contentItem, globalSize, currentTransform, bezierPathsBoundingBoxContext);
    }
    
    if (isInvertedMatte) {
        CGRect globalBounds = CGRect(0.0f, 0.0f, node->size().x, node->size().y).applyingTransform(currentTransform);
        if (globalRect) {
            globalRect = globalRect->unionWith(globalBounds);
        } else {
            globalRect = globalBounds;
        }
    }
    
    for (const auto &subNode : node->subnodes()) {
        auto subGlobalRect = getRenderNodeGlobalRect(subNode, globalSize, currentTransform, false, bezierPathsBoundingBoxContext);
        if (subGlobalRect) {
            if (globalRect) {
                globalRect = globalRect->unionWith(subGlobalRect.value());
            } else {
                globalRect = subGlobalRect.value();
            }
        }
    }
    
    if (globalRect) {
        CGRect integralGlobalRect(
            std::floor(globalRect->x),
            std::floor(globalRect->y),
            std::ceil(globalRect->width + globalRect->x - floor(globalRect->x)),
            std::ceil(globalRect->height + globalRect->y - floor(globalRect->y))
        );
        return integralGlobalRect.intersection(CGRect(0.0, 0.0, globalSize.x, globalSize.y));
    } else {
        return std::nullopt;
    }
}

namespace {

static void drawLottieContentItem(std::shared_ptr<Canvas> const &parentContext, std::shared_ptr<RenderTreeNodeContentItem> item, float parentAlpha, Vector2D const &globalSize, Transform2D const &parentTransform, BezierPathsBoundingBoxContext &bezierPathsBoundingBoxContext, CanvasRenderer::Configuration const &configuration) {
    auto currentTransform = parentTransform;
    Transform2D localTransform = item->transform;
    currentTransform = localTransform * currentTransform;
    
    float normalizedOpacity = item->alpha;
    float layerAlpha = ((float)normalizedOpacity) * parentAlpha;
    
    if (normalizedOpacity == 0.0f) {
        return;
    }
    
    parentContext->saveState();
    parentContext->concatenate(item->transform);
    
    std::shared_ptr<Canvas> const *currentContext;
    std::shared_ptr<Canvas> tempContext;
    
    bool needsTempContext = false;
    needsTempContext = layerAlpha != 1.0 && item->drawContentCount > 1;
    
    std::optional<CGRect> globalRect;
    if (needsTempContext) {
        if (configuration.canUseMoreMemory && globalSize.x <= minGlobalRectCalculationSize && globalSize.y <= minGlobalRectCalculationSize) {
            globalRect = CGRect(0.0, 0.0, globalSize.x, globalSize.y);
        } else {
            globalRect = getRenderContentItemGlobalRect(item, globalSize, parentTransform, bezierPathsBoundingBoxContext);
        }
        if (!globalRect || globalRect->width <= 0.0f || globalRect->height <= 0.0f) {
            parentContext->restoreState();
            return;
        }
        
        currentContext = &parentContext;
        (*currentContext)->pushLayer(globalRect.value(), layerAlpha, currentTransform);
    } else {
        currentContext = &parentContext;
    }
    
    float renderAlpha = 1.0;
    if (needsTempContext) {
        renderAlpha = 1.0;
    } else {
        renderAlpha = layerAlpha;
    }
    
    for (const auto &shading : item->shadings) {
        CanvasPathEnumerator iteratePaths;
        iteratePaths = [&](std::function<void(PathCommand const &)> &&iterate) {
            enumeratePaths(item, shading->subItemLimit, Transform2D::identity(), true, [&](BezierPath const &sourcePath, Transform2D const &transform) {
                auto path = sourcePath.copyUsingTransform(transform);
                
                PathCommand pathCommand;
                std::optional<PathElement> previousElement;
                for (const auto &element : path.elements()) {
                    if (previousElement.has_value()) {
                        if (previousElement->vertex.outTangentRelative().isZero() && element.vertex.inTangentRelative().isZero()) {
                            pathCommand.type = PathCommandType::LineTo;
                            pathCommand.points[0] = element.vertex.point;
                            iterate(pathCommand);
                        } else {
                            pathCommand.type = PathCommandType::CurveTo;
                            pathCommand.points[2] = element.vertex.point;
                            pathCommand.points[1] = element.vertex.inTangent;
                            pathCommand.points[0] = previousElement->vertex.outTangent;
                            iterate(pathCommand);
                        }
                    } else {
                        pathCommand.type = PathCommandType::MoveTo;
                        pathCommand.points[0] = element.vertex.point;
                        iterate(pathCommand);
                    }
                    previousElement = element;
                }
                if (path.closed().value_or(true)) {
                    pathCommand.type = PathCommandType::Close;
                    iterate(pathCommand);
                }
            });
        };
        
        if (shading->stroke) {
            if (shading->stroke->shading->type() == RenderTreeNodeContentItem::ShadingType::Solid) {
                RenderTreeNodeContentItem::SolidShading *solidShading = (RenderTreeNodeContentItem::SolidShading *)shading->stroke->shading.get();
                
                if (solidShading->opacity != 0.0) {
                    LineJoin lineJoin = LineJoin::Bevel;
                    switch (shading->stroke->lineJoin) {
                        case LineJoin::Bevel: {
                            lineJoin = LineJoin::Bevel;
                            break;
                        }
                        case LineJoin::Round: {
                            lineJoin = LineJoin::Round;
                            break;
                        }
                        case LineJoin::Miter: {
                            lineJoin = LineJoin::Miter;
                            break;
                        }
                        default: {
                            break;
                        }
                    }
                    
                    LineCap lineCap = LineCap::Square;
                    switch (shading->stroke->lineCap) {
                        case LineCap::Butt: {
                            lineCap = LineCap::Butt;
                            break;
                        }
                        case LineCap::Round: {
                            lineCap = LineCap::Round;
                            break;
                        }
                        case LineCap::Square: {
                            lineCap = LineCap::Square;
                            break;
                        }
                        default: {
                            break;
                        }
                    }
                    
                    std::vector<float> dashPattern;
                    if (!shading->stroke->dashPattern.empty()) {
                        dashPattern = shading->stroke->dashPattern;
                    }
                    
                    (*currentContext)->strokePath(iteratePaths, shading->stroke->lineWidth, lineJoin, lineCap, shading->stroke->dashPhase, dashPattern, Color(solidShading->color.r, solidShading->color.g, solidShading->color.b, solidShading->color.a * solidShading->opacity * renderAlpha));
                } else if (shading->stroke->shading->type() == RenderTreeNodeContentItem::ShadingType::Gradient) {
                    //TODO:gradient stroke
                }
            }
        } else if (shading->fill) {
            FillRule rule = FillRule::NonZeroWinding;
            switch (shading->fill->rule) {
                case FillRule::EvenOdd: {
                    rule = FillRule::EvenOdd;
                    break;
                }
                case FillRule::NonZeroWinding: {
                    rule = FillRule::NonZeroWinding;
                    break;
                }
                default: {
                    break;
                }
            }
            
            if (shading->fill->shading->type() == RenderTreeNodeContentItem::ShadingType::Solid) {
                RenderTreeNodeContentItem::SolidShading *solidShading = (RenderTreeNodeContentItem::SolidShading *)shading->fill->shading.get();
                if (solidShading->opacity != 0.0) {
                    (*currentContext)->fillPath(iteratePaths, rule, Color(solidShading->color.r, solidShading->color.g, solidShading->color.b, solidShading->color.a * solidShading->opacity * renderAlpha));
                }
            } else if (shading->fill->shading->type() == RenderTreeNodeContentItem::ShadingType::Gradient) {
                RenderTreeNodeContentItem::GradientShading *gradientShading = (RenderTreeNodeContentItem::GradientShading *)shading->fill->shading.get();
                
                if (gradientShading->opacity != 0.0) {
                    std::vector<Color> colors;
                    std::vector<float> locations;
                    for (const auto &color : gradientShading->colors) {
                        colors.push_back(Color(color.r, color.g, color.b, color.a * gradientShading->opacity * renderAlpha));
                    }
                    locations = gradientShading->locations;
                    
                    Gradient gradient(colors, locations);
                    Vector2D start(gradientShading->start.x, gradientShading->start.y);
                    Vector2D end(gradientShading->end.x, gradientShading->end.y);
                    
                    switch (gradientShading->gradientType) {
                        case GradientType::Linear: {
                            (*currentContext)->linearGradientFillPath(iteratePaths, rule, gradient, start, end);
                            break;
                        }
                        case GradientType::Radial: {
                            (*currentContext)->radialGradientFillPath(iteratePaths, rule, gradient, start, 0.0, start, start.distanceTo(end));
                            break;
                        }
                        default: {
                            break;
                        }
                    }
                }
            }
        }
    }
    
    for (auto it = item->subItems.rbegin(); it != item->subItems.rend(); it++) {
        const auto &subItem = *it;
        drawLottieContentItem(*currentContext, subItem, renderAlpha, globalSize, currentTransform, bezierPathsBoundingBoxContext, configuration);
    }
    
    if (needsTempContext) {
        (*currentContext)->popLayer();
    }    
    parentContext->restoreState();
}

static void renderLottieRenderNode(std::shared_ptr<RenderTreeNode> node, std::shared_ptr<Canvas> const &parentContext, Vector2D const &globalSize, Transform2D const &parentTransform, float parentAlpha, bool isInvertedMatte, BezierPathsBoundingBoxContext &bezierPathsBoundingBoxContext, CanvasRenderer::Configuration const &configuration) {
    float normalizedOpacity = node->alpha();
    float layerAlpha = ((float)normalizedOpacity) * parentAlpha;
    
    if (node->isHidden() || normalizedOpacity < minVisibleAlpha) {
        return;
    }
    
    auto currentTransform = parentTransform;
    Transform2D localTransform = node->transform();
    currentTransform = localTransform * currentTransform;
    
    std::shared_ptr<Canvas> maskContext;
    std::shared_ptr<Canvas> currentContext;
    std::shared_ptr<Canvas> tempContext;
    
    bool masksToBounds = node->masksToBounds();
    if (masksToBounds) {
        CGRect effectiveGlobalBounds = CGRect(0.0f, 0.0f, node->size().x, node->size().y).applyingTransform(currentTransform);
        if (effectiveGlobalBounds.width <= 0.0f || effectiveGlobalBounds.height <= 0.0f) {
            return;
        }
        if (effectiveGlobalBounds.contains(CGRect(0.0, 0.0, globalSize.x, globalSize.y))) {
            masksToBounds = false;
        }
    }
    
    parentContext->saveState();
    
    bool needsTempContext = false;
    if (node->mask() && !node->mask()->isHidden() && node->mask()->alpha() >= minVisibleAlpha) {
        needsTempContext = true;
    } else {
        needsTempContext = layerAlpha != 1.0 || masksToBounds;
    }
    
    std::optional<CGRect> globalRect;
    if (needsTempContext) {
        if (configuration.canUseMoreMemory && globalSize.x <= minGlobalRectCalculationSize && globalSize.y <= minGlobalRectCalculationSize) {
            globalRect = CGRect(0.0, 0.0, globalSize.x, globalSize.y);
        } else {
            globalRect = getRenderNodeGlobalRect(node, globalSize, parentTransform, false, bezierPathsBoundingBoxContext);
        }
        if (!globalRect || globalRect->width <= 0.0f || globalRect->height <= 0.0f) {
            parentContext->restoreState();
            return;
        }
        
        if ((node->mask() && !node->mask()->isHidden() && node->mask()->alpha() >= minVisibleAlpha) || masksToBounds) {
            auto maskBackingStorage = parentContext->makeLayer((int)(globalRect->width), (int)(globalRect->height));
            
            maskBackingStorage->concatenate(Transform2D::identity().translated(Vector2D(-globalRect->x, -globalRect->y)));
            maskBackingStorage->concatenate(currentTransform);
            
            if (masksToBounds) {
                maskBackingStorage->fill(CGRect(0.0f, 0.0f, node->size().x, node->size().y), Color(1.0f, 1.0f, 1.0f, 1.0f));
            }
            if (node->mask() && !node->mask()->isHidden() && node->mask()->alpha() >= minVisibleAlpha) {
                renderLottieRenderNode(node->mask(), maskBackingStorage, globalSize, currentTransform, 1.0, node->invertMask(), bezierPathsBoundingBoxContext, configuration);
            }
            
            maskContext = maskBackingStorage;
        }
        
        auto tempContextValue = parentContext->makeLayer((int)(globalRect->width), (int)(globalRect->height));
        tempContext = tempContextValue;
        
        currentContext = tempContextValue;
        currentContext->concatenate(Transform2D::identity().translated(Vector2D(-globalRect->x, -globalRect->y)));
        
        currentContext->saveState();
        currentContext->concatenate(currentTransform);
    } else {
        currentContext = parentContext;
    }
    
    parentContext->concatenate(node->transform());
    
    float renderAlpha = 1.0f;
    if (tempContext) {
        renderAlpha = 1.0f;
    } else {
        renderAlpha = layerAlpha;
    }
    
    if (node->_contentItem) {
        drawLottieContentItem(currentContext, node->_contentItem, renderAlpha, globalSize, currentTransform, bezierPathsBoundingBoxContext, configuration);
    }
    
    if (isInvertedMatte) {
        currentContext->fill(CGRect(0.0f, 0.0f, node->size().x, node->size().y), Color(0.0f, 0.0f, 0.0f, 1.0f));
        currentContext->setBlendMode(BlendMode::DestinationOut);
    }
    
    for (const auto &subnode : node->subnodes()) {
        renderLottieRenderNode(subnode, currentContext, globalSize, currentTransform, renderAlpha, false, bezierPathsBoundingBoxContext, configuration);
    }
    
    if (tempContext) {
        tempContext->restoreState();
        
        if (maskContext) {
            tempContext->setBlendMode(BlendMode::DestinationIn);
            tempContext->draw(maskContext, 1.0f, CGRect(globalRect->x, globalRect->y, globalRect->width, globalRect->height));
        }
        
        parentContext->concatenate(currentTransform.inverted());
        parentContext->draw(tempContext, layerAlpha, globalRect.value());
    }
    
    parentContext->restoreState();
}

}

class CanvasRenderer::Impl {
public:
    Impl() {
        _bezierPathsBoundingBoxContext = std::make_shared<BezierPathsBoundingBoxContext>();
    }
    
public:
    std::shared_ptr<BezierPathsBoundingBoxContext> bezierPathsBoundingBoxContext() const {
        return _bezierPathsBoundingBoxContext;
    }
    
private:
    std::shared_ptr<BezierPathsBoundingBoxContext> _bezierPathsBoundingBoxContext;
};

CanvasRenderer::CanvasRenderer() :
_impl(std::make_shared<Impl>()) {
}

void CanvasRenderer::render(std::shared_ptr<Renderer> renderer, std::shared_ptr<Canvas> canvas, Vector2D const &size, CanvasRenderer::Configuration const &configuration) {
    std::shared_ptr<RenderTreeNode> renderNode = renderer->renderNode();
    if (!renderNode) {
        return;
    }
    
    Vector2D scale = Vector2D(size.x / (float)renderer->size().x, size.y / (float)renderer->size().y);
    canvas->saveState();
    canvas->concatenate(Transform2D::makeScale(scale.x, scale.y));
    
    Transform2D rootTransform = Transform2D::identity().scaled(Vector2D(size.x / (float)renderer->size().x, size.y / (float)renderer->size().y));
    renderLottieRenderNode(renderNode, canvas, size, rootTransform, 1.0, false, *_impl->bezierPathsBoundingBoxContext().get(), configuration);
    
    canvas->restoreState();
}

}
