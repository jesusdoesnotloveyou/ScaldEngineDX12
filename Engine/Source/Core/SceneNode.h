#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <DirectXMath.h>
#include <DirectXCollision.h>

namespace Scald
{
    using namespace DirectX;

    class Mesh;
    class CommandList;
    class Visitor;

    class SceneNode : public std::enable_shared_from_this<SceneNode>
    {
    public:
        explicit SceneNode( const XMMATRIX& localTransform = XMMatrixIdentity() );
        virtual ~SceneNode();

        /**
         * Assign a name to the scene node so it can be searched for later.
         */
        const std::string& GetName() const;
        void SetName( const std::string& name );

        /**
         * Get the scene nodes local (relative to its parent's transform).
         */
        XMMATRIX GetLocalTransform() const;
        void SetLocalTransform( const XMMATRIX& localTransform );

        /**
         * Get the inverse of the local transform.
         */
        XMMATRIX GetInverseLocalTransform() const;

        /**
         * Get the scene node's world transform (concatenated with its parents
         * world transform).
         */
        XMMATRIX GetWorldTransform() const;

        /**
         * Get the inverse of the world transform (concatenated with its parent's
         * world transform).
         */
        XMMATRIX GetInverseWorldTransform() const;

        /**
         * Add a child node to this scene node.
         * NOTE: Circular references are not checked.
         * A scene node "owns" its children. If the root node
         * is deleted, all of its children are deleted if nothing
         * else is referencing them.
         */
        void AddChild( std::shared_ptr<SceneNode> childNode );
        void RemoveChild( std::shared_ptr<SceneNode> childNode );
        void SetParent( std::shared_ptr<SceneNode> parentNode );

        /**
         * Add a mesh to this scene node.
         * @returns The index of the mesh in the mesh list.
         */
        size_t AddMesh( std::shared_ptr<Mesh> mesh );
        void RemoveMesh( std::shared_ptr<Mesh> mesh );

        /**
         * Get a mesh in the list of meshes for this node.
         */
        std::shared_ptr<Mesh> GetMesh( size_t index = 0 );

        /**
         * Get the AABB for this scene node.
         * The AABB is formed from the combination of all mesh AABB's.
         */
        const BoundingBox& GetAABB() const;

        /**
         * Accept a visitor.
         */
        void Accept( Visitor& visitor );

    protected:
        XMMATRIX GetParentWorldTransform() const;

    private:
        using NodePtr     = std::shared_ptr<SceneNode>;
        using NodeList    = std::vector<NodePtr>;
        using NodeNameMap = std::multimap<std::string, NodePtr>;
        using MeshList    = std::vector<std::shared_ptr<Mesh>>;

        std::string m_Name;

        // This data must be aligned to a 16-byte boundary.
        // The only way to guarantee this, is to allocate this
        // structure in aligned memory.
        struct alignas( 16 ) AlignedData
        {
            XMMATRIX m_LocalTransform;
            XMMATRIX m_InverseTransform;
        } * m_AlignedData;

        std::weak_ptr<SceneNode> m_ParentNode;
        NodeList                m_Children;
        NodeNameMap             m_ChildrenByName;
        MeshList                m_Meshes;

        // The AABB for this scene node. 
        // Created by merging the AABB of the meshes.
        BoundingBox m_AABB;
    };
}  // namespace Scald