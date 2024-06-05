#include <LottieCpp/CanvasRenderer.h>

namespace lottie {

class CanvasRenderer::Impl {
public:
    Impl() {
        _bezierPathsBoundingBoxContext = std::make_shared<lottie::BezierPathsBoundingBoxContext>();
    }
    
public:
    std::shared_ptr<lottie::BezierPathsBoundingBoxContext> bezierPathsBoundingBoxContext() const {
        return _bezierPathsBoundingBoxContext;
    }
    
private:
    std::shared_ptr<lottie::BezierPathsBoundingBoxContext> _bezierPathsBoundingBoxContext;
};

CanvasRenderer::CanvasRenderer() :
_impl(std::make_shared<Impl>()) {
}



}
