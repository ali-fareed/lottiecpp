#ifndef NodeOutput_hpp
#define NodeOutput_hpp

#include <memory>

namespace lottie {

/// Defines the basic outputs of an animator node.
///
class NodeOutput {
public:
    /// The parent node.
    virtual std::shared_ptr<NodeOutput> parent() = 0;
    
    /// Returns true if there are any updates upstream.
    virtual bool hasOutputUpdates(float forFrame) = 0;
    
    virtual bool isEnabled() const = 0;
    virtual void setIsEnabled(bool isEnabled) = 0;
};

}

#endif /* NodeOutput_hpp */
