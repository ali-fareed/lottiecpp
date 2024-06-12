#ifndef NullCanvasImpl_h
#define NullCanvasImpl_h

#ifdef __cplusplus

#include <LottieCpp/LottieCpp.h>

namespace lottie {

class NullCanvasImpl: public Canvas {
public:
    NullCanvasImpl(int width, int height);
    virtual ~NullCanvasImpl();
    
    virtual std::shared_ptr<Canvas> makeLayer(int width, int height) override;
    
    virtual void saveState() override;
    virtual void restoreState() override;
    
    virtual void fillPath(CanvasPathEnumerator const &enumeratePath, lottie::FillRule fillRule, lottie::Color const &color) override;
    virtual void linearGradientFillPath(CanvasPathEnumerator const &enumeratePath, lottie::FillRule fillRule, lottie::Gradient const &gradient, lottie::Vector2D const &start, lottie::Vector2D const &end) override;
    virtual void radialGradientFillPath(CanvasPathEnumerator const &enumeratePath, lottie::FillRule fillRule, lottie::Gradient const &gradient, lottie::Vector2D const &startCenter, float startRadius, lottie::Vector2D const &endCenter, float endRadius) override;
    virtual void strokePath(CanvasPathEnumerator const &enumeratePath, float lineWidth, lottie::LineJoin lineJoin, lottie::LineCap lineCap, float dashPhase, std::vector<float> const &dashPattern, lottie::Color const &color) override;
    virtual void linearGradientStrokePath(CanvasPathEnumerator const &enumeratePath, float lineWidth, lottie::LineJoin lineJoin, lottie::LineCap lineCap, float dashPhase, std::vector<float> const &dashPattern, Gradient const &gradient, lottie::Vector2D const &start, lottie::Vector2D const &end) override;
    virtual void radialGradientStrokePath(CanvasPathEnumerator const &enumeratePath, float lineWidth, lottie::LineJoin lineJoin, lottie::LineCap lineCap, float dashPhase, std::vector<float> const &dashPattern, Gradient const &gradient, lottie::Vector2D const &startCenter, float startRadius, lottie::Vector2D const &endCenter, float endRadius) override;
    virtual void fill(lottie::CGRect const &rect, lottie::Color const &fillColor) override;
    
    virtual void setBlendMode(BlendMode blendMode) override;
    
    virtual void concatenate(lottie::Transform2D const &transform) override;
    
    virtual void draw(std::shared_ptr<Canvas> const &other, float alpha, lottie::CGRect const &rect) override;
    
    void flush();

private:
    lottie::Transform2D _transform;
};

}

#endif

#endif
