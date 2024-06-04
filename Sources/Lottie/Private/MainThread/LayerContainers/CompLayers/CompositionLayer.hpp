#ifndef CompositionLayer_hpp
#define CompositionLayer_hpp

#include <LottieCpp/Vectors.h>
#include "Lottie/Public/Primitives/CALayer.hpp"
#include "Lottie/Private/MainThread/NodeRenderSystem/NodeProperties/Protocols/KeypathSearchable.hpp"
#include "Lottie/Private/Model/Layers/LayerModel.hpp"
#include "Lottie/Private/MainThread/LayerContainers/Utility/LayerTransformNode.hpp"
#include "Lottie/Private/MainThread/LayerContainers/CompLayers/MaskContainerLayer.hpp"

#include <memory>

namespace lottie {

class CompositionLayer;
class InvertedMatteLayer;

/// A layer that inverses the alpha output of its input layer.
class InvertedMatteLayer: public CALayer {
public:
    InvertedMatteLayer(std::shared_ptr<CompositionLayer> inputMatte);
    
    std::shared_ptr<CompositionLayer> _inputMatte;
    
    virtual bool isInvertedMatte() const override {
        return true;
    }
};

std::shared_ptr<InvertedMatteLayer> makeInvertedMatteLayer(std::shared_ptr<CompositionLayer> compositionLayer);

/// The base class for a child layer of CompositionContainer
class CompositionLayer: public CALayer, public KeypathSearchable {
public:
    CompositionLayer(std::shared_ptr<LayerModel> const &layer, Vector2D size) {
        _contentsLayer = std::make_shared<CALayer>();
        
        _transformNode = std::make_shared<LayerTransformNode>(layer->transform);
        
        if (layer->masks.has_value()) {
            _maskLayer = std::make_shared<MaskContainerLayer>(layer->masks.value());
        } else {
            _maskLayer = nullptr;
        }
        
        _matteType = layer->matte;
        
        _inFrame = layer->inFrame;
        _outFrame = layer->outFrame;
        _timeStretch = layer->timeStretch();
        _startFrame = layer->startTime;
        if (layer->name.has_value()) {
            _keypathName = layer->name.value();
        } else {
            _keypathName = "Layer";
        }
        
        _childKeypaths.push_back(_transformNode->transformProperties());
        
        _contentsLayer->setSize(size);
        
        if (layer->blendMode.has_value() && layer->blendMode.value() != BlendMode::Normal) {
            setCompositingFilter(layer->blendMode);
        }
        
        addSublayer(_contentsLayer);
        
        if (_maskLayer) {
            _contentsLayer->setMask(_maskLayer);
        }
    }
    
    virtual std::string keypathName() const override {
        return _keypathName;
    }
    
    virtual std::map<std::string, std::shared_ptr<AnyNodeProperty>> keypathProperties() const override {
        return {};
    }
    
    virtual std::shared_ptr<CALayer> keypathLayer() const override {
        return _contentsLayer;
    }
    
    void displayWithFrame(float frame, bool forceUpdates, BezierPathsBoundingBoxContext &boundingBoxContext) {
        bool layerVisible = isInRangeOrEqual(frame, _inFrame, _outFrame);
        
        if (_transformNode->updateTree(frame, forceUpdates) || _contentsLayer->isHidden() != !layerVisible) {
            _contentsLayer->setTransform(_transformNode->globalTransform());
            _contentsLayer->setOpacity(_transformNode->opacity());
            _contentsLayer->setIsHidden(!layerVisible);
            
            updateContentsLayerParameters();
        }
        
        /// Only update contents if current time is within the layers time bounds.
        if (layerVisible) {
            displayContentsWithFrame(frame, forceUpdates, boundingBoxContext);
            if (_maskLayer) {
                _maskLayer->updateWithFrame(frame, forceUpdates);
            }
        }
    }
    
    virtual void updateContentsLayerParameters() {
    }
    
    virtual void displayContentsWithFrame(float frame, bool forceUpdates, BezierPathsBoundingBoxContext &boundingBoxContext) {
        /// To be overridden by subclass
    }
    
    
    virtual std::vector<std::shared_ptr<KeypathSearchable>> const &childKeypaths() const override {
        return _childKeypaths;
    }
    
    std::shared_ptr<CompositionLayer> _matteLayer;
    void setMatteLayer(std::shared_ptr<CompositionLayer> matteLayer) {
        _matteLayer = matteLayer;
        if (matteLayer) {
            if (_matteType.has_value() && _matteType.value() == MatteType::Invert) {
                setMask(makeInvertedMatteLayer(matteLayer));
            } else {
                setMask(matteLayer);
            }
        } else {
            setMask(nullptr);
        }
    }
    
    std::shared_ptr<CALayer> const &contentsLayer() const {
        return _contentsLayer;
    }
    
    std::shared_ptr<MaskContainerLayer> const &maskLayer() const {
        return _maskLayer;
    }
    void setMaskLayer(std::shared_ptr<MaskContainerLayer> const &maskLayer) {
        _maskLayer = maskLayer;
    }
    
    std::optional<MatteType> const &matteType() const {
        return _matteType;
    }
    
    float inFrame() const {
        return _inFrame;
    }
    float outFrame() const {
        return _outFrame;
    }
    float startFrame() const {
        return _startFrame;
    }
    float timeStretch() const {
        return _timeStretch;
    }
    
    virtual std::shared_ptr<RenderTreeNode> renderTreeNode(BezierPathsBoundingBoxContext &boundingBoxContext) {
        return nullptr;
    }
    
public:
    std::shared_ptr<LayerTransformNode> const transformNode() const {
        return _transformNode;
    }
    
protected:
    std::shared_ptr<CALayer> _contentsLayer;
    std::optional<MatteType> _matteType;
    
private:
    std::shared_ptr<LayerTransformNode> _transformNode;
    
    std::shared_ptr<MaskContainerLayer> _maskLayer;
    
    float _inFrame = 0.0;
    float _outFrame = 0.0;
    float _startFrame = 0.0;
    float _timeStretch = 0.0;
    
    // MARK: Keypath Searchable
    
    std::string _keypathName;
    
public:
    virtual bool isImageCompositionLayer() const {
        return false;
    }
    
    virtual bool isTextCompositionLayer() const {
        return false;
    }
    
protected:
    std::vector<std::shared_ptr<KeypathSearchable>> _childKeypaths;
};

}

#endif /* CompositionLayer_hpp */
