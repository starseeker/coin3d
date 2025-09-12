#ifndef COIN_SBMODERNUTILS_H
#define COIN_SBMODERNUTILS_H

/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 * 
 * Phase 3 Implementation Modernization: Modern C++17 Utility Functions
 * 
 * This header provides modern C++17 utilities for Coin3D while maintaining
 * full backward compatibility with existing APIs.
 \**************************************************************************/

#include <Inventor/SbBasic.h>
#include <optional>
#include <string_view>
#include <memory>

// Forward declarations
class SoNode;
class SbName;

/* ********************************************************************** */

/*!
 * \brief Modern C++17 utilities for enhanced Coin3D functionality
 * 
 * This namespace provides modern C++17 features as convenience functions
 * that complement the existing Coin3D API without breaking compatibility.
 */
namespace SbModernUtils {

/*!
 * \brief Optional node lookup with modern std::optional return type
 * 
 * This function provides a modern alternative to SoNode::getByName that
 * returns std::optional<SoNode*> instead of a raw pointer that might be NULL.
 * 
 * \param name The name of the node to find
 * \return std::optional containing the node if found, std::nullopt otherwise
 * 
 * Example usage:
 * \code
 * if (auto node = SbModernUtils::findNodeByName("myNode")) {
 *   // Use node.value() safely
 *   node.value()->ref();
 * }
 * \endcode
 */
COIN_DLL_API std::optional<SoNode*> findNodeByName(const SbName & name);

/*!
 * \brief String view-based name comparison for efficient string operations
 * 
 * This function allows comparing SbName objects with string literals or
 * std::string_view for more efficient string operations without allocations.
 * 
 * \param name The SbName to compare
 * \param str The string view to compare against  
 * \return true if the name matches the string view
 * 
 * Example usage:
 * \code
 * if (SbModernUtils::nameEquals(node->getName(), "Transform")) {
 *   // Handle transform node
 * }
 * \endcode
 */
COIN_DLL_API bool nameEquals(const SbName & name, std::string_view str);

/*!
 * \brief Enhanced RAII wrapper for SoNode reference counting
 * 
 * This class provides automatic reference counting for SoNode objects
 * using modern C++17 RAII patterns. It ensures proper cleanup even
 * in the presence of exceptions.
 * 
 * Example usage:
 * \code
 * auto nodeRef = SbModernUtils::makeNodeRef(node);
 * // Node is automatically unreferenced when nodeRef goes out of scope
 * \endcode
 */
class COIN_DLL_API SoNodeRef {
public:
    explicit SoNodeRef(SoNode* node);
    ~SoNodeRef();
    
    // Non-copyable but movable
    SoNodeRef(const SoNodeRef&) = delete;
    SoNodeRef& operator=(const SoNodeRef&) = delete;
    SoNodeRef(SoNodeRef&& other) noexcept;
    SoNodeRef& operator=(SoNodeRef&& other) noexcept;
    
    SoNode* get() const { return node_; }
    SoNode* operator->() const { return node_; }
    SoNode& operator*() const { return *node_; }
    
    explicit operator bool() const { return node_ != nullptr; }
    
    // Release ownership without unreferencing
    SoNode* release();
    
private:
    SoNode* node_;
};

/*!
 * \brief Factory function for creating SoNodeRef objects
 * 
 * \param node The node to manage (must not be NULL)
 * \return SoNodeRef object managing the node's reference count
 */
COIN_DLL_API SoNodeRef makeNodeRef(SoNode* node);

/*!
 * \brief Create a unique_ptr-like wrapper for any object with ref/unref
 * 
 * This template provides a std::unique_ptr-like interface for objects
 * that use reference counting instead of direct deletion.
 * 
 * \tparam T The type of object to manage (must have ref() and unref() methods)
 */
template<typename T>
class RefCountedPtr {
public:
    explicit RefCountedPtr(T* ptr = nullptr) : ptr_(ptr) {
        if (ptr_) ptr_->ref();
    }
    
    ~RefCountedPtr() {
        if (ptr_) ptr_->unref();
    }
    
    // Non-copyable but movable  
    RefCountedPtr(const RefCountedPtr&) = delete;
    RefCountedPtr& operator=(const RefCountedPtr&) = delete;
    
    RefCountedPtr(RefCountedPtr&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }
    
    RefCountedPtr& operator=(RefCountedPtr&& other) noexcept {
        if (this != &other) {
            if (ptr_) ptr_->unref();
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }
    
    T* get() const { return ptr_; }
    T* operator->() const { return ptr_; }
    T& operator*() const { return *ptr_; }
    
    explicit operator bool() const { return ptr_ != nullptr; }
    
    T* release() {
        T* result = ptr_;
        ptr_ = nullptr;
        return result;
    }
    
    void reset(T* ptr = nullptr) {
        if (ptr_) ptr_->unref();
        ptr_ = ptr;
        if (ptr_) ptr_->ref();
    }
    
private:
    T* ptr_;
};

/*!
 * \brief Factory function for creating RefCountedPtr objects
 * 
 * \tparam T The type of object to manage
 * \param ptr The object to manage
 * \return RefCountedPtr managing the object
 */
template<typename T>
RefCountedPtr<T> makeRefCountedPtr(T* ptr) {
    return RefCountedPtr<T>(ptr);
}

} // namespace SbModernUtils

#endif // !COIN_SBMODERNUTILS_H