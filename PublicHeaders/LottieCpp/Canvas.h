#ifndef Canvas_h
#define Canvas_h

#ifdef __cplusplus

#include <memory>
#include <vector>
#include <cassert>
#include <functional>

namespace lottie {

class Gradient {
public:
    Gradient(std::vector<Color> const &colors, std::vector<float> const &locations) :
    _colors(colors),
    _locations(locations) {
#if DEBUG
        assert(_colors.size() == _locations.size());
#endif
    }
    
    std::vector<Color> const &colors() const {
        return _colors;
    }
    
    std::vector<float> const &locations() const {
        return _locations;
    }
    
private:
    std::vector<Color> _colors;
    std::vector<float> _locations;
};

enum class BlendMode {
    Normal,
    DestinationIn,
    DestinationOut
};

enum class PathCommandType {
    MoveTo,
    LineTo,
    CurveTo,
    Close
};

typedef struct {
    PathCommandType type;
    Vector2D points[4];
} PathCommand;

typedef std::function<void(std::function<void(PathCommand const &)> &&)> CanvasPathEnumerator;

class Canvas {
public:
    virtual ~Canvas() = default;
    
    virtual int width() const = 0;
    virtual int height() const = 0;
    
    virtual std::shared_ptr<Canvas> makeLayer(int width, int height) = 0;
    
    virtual void saveState() = 0;
    virtual void restoreState() = 0;
    
    virtual void fillPath(CanvasPathEnumerator const &enumeratePath, FillRule fillRule, Color const &color) = 0;
    virtual void linearGradientFillPath(CanvasPathEnumerator const &enumeratePath, FillRule fillRule, Gradient const &gradient, Vector2D const &start, Vector2D const &end) = 0;
    virtual void radialGradientFillPath(CanvasPathEnumerator const &enumeratePath, FillRule fillRule, Gradient const &gradient, Vector2D const &startCenter, float startRadius, Vector2D const &endCenter, float endRadius) = 0;
    
    virtual void strokePath(CanvasPathEnumerator const &enumeratePath, float lineWidth, LineJoin lineJoin, LineCap lineCap, float dashPhase, std::vector<float> const &dashPattern, Color const &color) = 0;
    virtual void linearGradientStrokePath(CanvasPathEnumerator const &enumeratePath, float lineWidth, LineJoin lineJoin, LineCap lineCap, float dashPhase, std::vector<float> const &dashPattern, Gradient const &gradient, Vector2D const &start, Vector2D const &end) = 0;
    virtual void radialGradientStrokePath(CanvasPathEnumerator const &enumeratePath, float lineWidth, LineJoin lineJoin, LineCap lineCap, float dashPhase, std::vector<float> const &dashPattern, Gradient const &gradient, Vector2D const &startCenter, float startRadius, Vector2D const &endCenter, float endRadius) = 0;
    
    virtual void fill(CGRect const &rect, Color const &fillColor) = 0;
    virtual void setBlendMode(BlendMode blendMode) = 0;
    
    virtual void setAlpha(float alpha) = 0;
    
    virtual void concatenate(Transform2D const &transform) = 0;
    
    virtual void draw(std::shared_ptr<Canvas> const &other, CGRect const &rect) = 0;
};

}

#endif

#endif

