#pragma once

#include "ovvizIVisualizationWidget.h"

typedef struct _GtkWidget GtkWidget;
typedef struct _GtkTreeView GtkTreeView;
typedef struct _GtkTreeIter GtkTreeIter;
typedef struct _GtkTreePath GtkTreePath;

namespace OpenViBE {
namespace VisualizationToolkit {
/**
	 * \brief Drag locations around a widget
	 */
enum class EDragLocation { Left, Right, Top, Bottom };

/**
	 * \brief Column types in a visualization tree
	 */
enum class EVisualizationTreeColumn
{
	/**
	 * \brief Node name
	 * Depending on the node type, it refers to :
	 * the name given to the window for EVisualizationTreeNode::VisualizationWindow nodes,
	 * the name of the notebook tab for EVisualizationTreeNode::VisualizationPanel nodes,
	 * the name of the related IBox for EVisualizationTreeNode::VisualizationBox nodes,
	 * "Vertical split" for EVisualizationTreeNode::VerticalSplit nodes,
	 * "Horizontal split" for EVisualizationTreeNode::HorizontalSplit nodes,
	 * "Unaffected display plugins" for the EVisualizationTreeNode::Unaffected node
	 */
	StringName,
	/**
	 * \brief Icon associated to a node
	 * Constant except for type EVisualizationTreeNode::VisualizationBox, for which it can be specified in the plugin descriptor
	 */
	StringStockIcon,
	ULongNodeType,		///< EVisualizationTreeNode type
	StringIdentifier,	///< IVisualizationWidget identifier
	PointerWidget		///< Pointer to the GtkWidget associated to the node (if any)
};

/**
	 * \brief Node types in a visualization tree
	 */
enum class EVisualizationTreeNode
{
	Undefined,				///< May be used for placeholders
	VPU,					///< Not used yet
	VisualizationWindow,	///< A top level window
	VisualizationPanel,		///< A notebook tab inside a window
	VisualizationBox,		///< A visualization box (a plugin which displays something)
	VerticalSplit,			///< A vertical split widget
	HorizontalSplit,		///< A horizontal split widget
	Unaffected				///< A special node parent to all unaffected visualization boxes
};

class IVisualizationTree;

/**
	 * \brief Tree view interface
	 * It must be implemented by classes which communicate with the IVisualizationTree class in order
	 * to display the contents of the visualization tree.
	 */
struct ITreeViewCB
{
	virtual ~ITreeViewCB() = default;

	/**
	 * \brief Notifies the tree view that a new widget is being created
	 * \param widget pointer to the newly created widget
	 */
	virtual void CreateTreeWidget(IVisualizationWidget* widget) { }

	/**
	 * \brief Notifies the tree view that a widget is being loaded.
	 * This method must return a Gtk widget that matches the description passed in parameter, or nullptr if
	 * it doesn't wish to support a specific kind of widget (e.g. top level windows are not instantiated by the
	 * offline window manager). The widget pointer returned is stored in the EVisualizationTreeColumn::PointerWidget
	 * column of the corresponding tree node.
	 * \param widget pointer to loaded visualization widget
	 * \return GtkWidget* pointer to Gtk widget associated to visualization widget (possibly nullptr)
	 */
	virtual GtkWidget* LoadTreeWidget(IVisualizationWidget* widget) { return nullptr; }

	/**
	 * \brief Notifies the tree view that a widget hierarchy has been loaded
	 * This method lets the tree view perform additional loading operations at the end of the loading
	 * process
	 * \param widget pointer to visualization widget
	 */
	virtual void EndLoadTreeWidget(IVisualizationWidget* widget) { }

	/**
	 * \brief Asks for the visualization (visible) widget associated to the tree widget passed in parameter
	 * In some cases the Gtk 'tree widget' associated to a tree node is a table comprising several widgets.
	 * For example, in the offline window manager, a widget is often stored in the central cell of a 3x3 table and is surrounded
	 * by transparent event boxes for drag n' drop purposes.
	 * This callback returns the central 'visualization widget' (the visible widget) contained in such a table if any, otherwise
	 * it returns the tree widget itself.
	 * \param widget pointer to the Gtk widget associated to a tree node (as stored in the EVisualizationTreeColumn::PointerWidget
	 * column of a tree store)
	 * \return pointer to the visible Gtk widget if pTreeWidget is a table, pTreeWidget itself otherwise(default)
	 * \sa getTreeWidget()
	 */
	virtual GtkWidget* GetVisualizationWidget(GtkWidget* widget) { return widget; }

	/**
	 * \brief Asks for the tree widget associated to a visualization widget
	 * In some cases a Gtk widget is inserted in a 3x3 table for drag n' drop purposes. In such cases, a distinction is
	 * made between the 'visualization widget' (the actual visible widget) and the widget stored in the EVisualizationTreeColumn::PointerWidget
	 * column of a tree store.
	 * \param widget pointer to visualization widget
	 * \return parent table if any, visualizationWidget otherwise (default)
	 * \sa getVisualizationWidget()
	 */
	virtual GtkWidget* GetTreeWidget(GtkWidget* widget) { return widget; }

	/**
	 * \brief Icon associated to a visualization tree node
	 * Each node has a Gtk icon. It is defined by the tree view, but can be customised for nodes of type EVisualizationTreeNode::VisualizationBox.
	 * This method asks the tree view what icon is to be used for a given type of node.
	 * \param nodeType type of node whose icon name is to be retrieved
	 * \return name of stock icon to be associated to the type of node passed in parameter
	 */
	virtual const char* GetTreeWidgetIcon(EVisualizationTreeNode nodeType) { return ""; }

	/**
	 * \brief Set toolbar pointer of a visualization box
	 * Forwards pointer to the toolbar of a visualization box as the player is launched and each visualization
	 * box contained in the active scenario is being instantiated.
	 * \param boxID Identifier of IBox whose toolbar is being set
	 * \param toolbar pointer to toolbar of visualization box
	 * \return true if widget was successfully registered, false otherwise
	 */
	virtual bool SetToolbar(const CIdentifier& boxID, GtkWidget* toolbar) { return false; }

	/**
	 * \brief Set topmost widget pointer of a visualization box
	 * Forwards pointer to the topmost widget of a visualization box as the player is launched and each visualization
	 * box contained in the active scenario is being instantiated. Used to position each box at the appropriate position, as defined
	   * using the offline window manager.
	 * \param boxID Identifier of IBox whose toolbar is being set
	 * \param widget pointer to main window of visualization box
	 * \return true if widget was successfully registered, false otherwise
	 */
	virtual bool SetWidget(const CIdentifier& boxID, GtkWidget* widget) { return false; }
};

/**
	 * \class IVisualizationTree
	 * \author Vincent Delannoy (INRIA/IRISA)
	 * \date 2007-11
	 * \brief Handles visualization widgets and their arrangement in space
	 * This interface lets applications configure visualization widgets by adding them to windows and
	 * tabs, positioning them next to each other using paned widgets, and resizing them as the user
	 * sees fit. All widgets are saved in a symbolic way as IVisualizationWidget instances referenced in a tree store.
	 * Methods of this class are essentially meant to be called from an external class inheriting from the ITreeViewCB
	 * interface, for offline design or online use of the widgets.
	 */
class IVisualizationTree
{
public:
	virtual ~IVisualizationTree() = default;
	/**
	 * \brief Initializes the visualization tree
	 * This method registers the scenario associated to this tree and creates a tree store.
	 * \param scenario scenario associated to this tree store
	 * \return true if tree was successfully initialized, false otherwise
	 */
	virtual bool init(const Kernel::IScenario* scenario) = 0;

	/**
	 * \name IVisualizationWidget management
	 */
	//@{

	/**
	 * \brief Iterates through IVisualizationWidget instances managed by this tree
	 * Upon first call, this method should be passed an identifier initialized with CIdentifier::undefined().
	 * It is modified at each subsequent call until all widgets have been returned (in which
	 * case the identifier is reset to OV_Undefined)
	 * \param id [in/out] identifier of current visualization widget when calling the method and of next widget upon return
	 * \return true if a widget was found, false if past beyond last one
	 */
	virtual bool getNextVisualizationWidgetIdentifier(CIdentifier& id) const = 0;

	/**
	 * \brief Iterates through IVisualizationWidget instances of type type managed by this tree
	 * \param id [in/out] identifier of current visualization widget when calling the method and of next widget upon return
	 * \param type restricts search to IVisualizationWidget instances of type type
	 * \return true if a widget was found, false if past beyond last one
	 * \sa getNextVisualizationWidgetIdentifier()
	 */
	virtual bool getNextVisualizationWidgetIdentifier(CIdentifier& id, EVisualizationWidget type) const = 0;

	/**
	 * \brief Tests whether an identifier corresponds to an IVisualizationWidget instance
	 * This method browses the internal tree store to look for a node whose EVisualizationTreeColumn::StringIdentifier field
	 * equals 'identifier' once converted to CString format.
	 * \param id identifier to look for in the internal tree store
	 * \return true if identifier corresponds to an IVisualizationWidget instance stored in this tree, false otherwise
	 */
	virtual bool isVisualizationWidget(const CIdentifier& id) const = 0;

	/**
	 * \brief Returns the IVisualizationWidget instance whose identifier is passed in parameter
	 * \param id identifier to look for in the internal tree store
	 * \return pointer to IVisualizationWidget whose identifier matches 'identifier' if any, nullptr otherwise
	 */
	virtual IVisualizationWidget* getVisualizationWidget(const CIdentifier& id) const = 0;

	/**
	 * \brief Returns the IVisualizationWidget instance whose corresponding IBox identifier matches 'boxID'
	 * This method inherently restricts the search to nodes of type EVisualizationTreeNode::VisualizationBox
	 * since only these nodes are associated to an IBox instance.
	 * \param boxID identifier of IBox whose associated IVisualizationWidget is to be retrieved
	 * \return IVisualizationWidget whose associated IBox identifier matches 'boxID' if any, nullptr otherwise
	 */
	virtual IVisualizationWidget* getVisualizationWidgetFromBoxIdentifier(const CIdentifier& boxID) const = 0;

	/**
	 * \brief Adds an IVisualizationWidget instance to the tree
	 * Seven parameters describe the IVisualizationWidget to be created. Not all are relevant for every type of widget.
	 * \param id identifier of the widget to be created
	 * \param name name of the widget
	 * \param type type of the widget
	 * \param parentID parent widget identifier (CIdentifier::undefined() for top-level widgets)
	 * \param parentIdx index where this widget is to be parented (irrelevant for top-level widgets)
	 * \param boxID identifier of associated IBox (for widgets of type EVisualizationWidget::Box only)
	 * \param nChild number of children of this widget (none for a visualization box, 1 for a visualization panel, 2 for split widgets, variable number for windows)
	 * \param suggestedID a suggestion as to the identifier to use
	 * \return true if widget successfully added to the internal tree store, false otherwise
	 */
	virtual bool addVisualizationWidget(CIdentifier& id, const CString& name, EVisualizationWidget type, const CIdentifier& parentID, size_t parentIdx,
										const CIdentifier& boxID, size_t nChild, const CIdentifier& suggestedID) = 0;

	/**
	 * \brief Returns the index where a widget is parented
	 * Irrelevant for top level windows.
	 * \param id identifier of the IVisualizationWidget whose index is to be retrieved
	 * \param index [out] index where the widget is parented
	 * \return true if widget index could be determined, false otherwise
	 */
	virtual bool getVisualizationWidgetIndex(const CIdentifier& id, size_t& index) const = 0;

	/**
	 * \brief Unparents a widget from its parent, if any
	 * \param id identifier of widget to be unparented
	 * \param index [out] index where this widget was parented
	 * \return true if widget could be removed from its parent, false otherwise
	 */
	virtual bool unparentVisualizationWidget(const CIdentifier& id, size_t& index) = 0;

	/**
	 * \brief Parents a widget to a parent widget
	 * \param id identifier of widget to be parented
	 * \param parentID identifier of parent widget
	 * \param index index where widget is to be parented
	 * \return true if widget could be parented as desired, false otherwise
	 */
	virtual bool parentVisualizationWidget(const CIdentifier& id, const CIdentifier& parentID, const size_t index) = 0;

	/**
	 * \brief Destroys a widget hierarchy
	 * \param id identifier of widget that is to be destroyed, along with all widgets in its subtree
	 * \param destroy if false, widgets of type EVisualizationTreeNode::VisualizationBox are unaffected only (as opposed to destroyed)
	 * \return true if hierarchy was successfully destroyed, false otherwise
	 */
	virtual bool destroyHierarchy(const CIdentifier& id, bool destroy = true) = 0;
	//@}

	/**
	 * \name Tree view creation/registration
	 */
	//@{

	/**
	 * \brief Creates a tree view from the internal tree store/model
	 * \return pointer to newly created tree view
	 */
	virtual GtkTreeView* createTreeViewWithModel() = 0;

	/**
	 * \brief Sets the instance implementing the ITreeViewCB interface that is to be used with the visualization tree
	 * \param callback pointer to an implementation of the ITreeViewCB interface
	 * \return true if tree view was successfully registered, false otherwise
	 * \sa ITreeViewCB
	 */
	virtual bool setTreeViewCB(ITreeViewCB* callback) = 0;
	//@}

	/**
	 * \brief Recreates Gtk widgets based on the widgets description stored in the internal tree store.
	 * To be called upon widget creation, deletion or repositioning. Side effects : all tree iterators
	 * and GtkWidget pointers are invalidated and replaced with new values.
	 * \return true if widgets were successfully reloaded, false otherwise
	 */
	virtual bool reloadTree() = 0;

	/**
	 * \name Helper functions
	 */
	//@{

	/**
	 * \brief Returns currently selected node
	 * \param treeView pointer to tree view whose selected node is to be retrieved
	 * \param iter [out] pointer to selected node
	 * \return true if a node is selected, false otherwise
	 */
	virtual bool getTreeSelection(GtkTreeView* treeView, GtkTreeIter* iter) = 0;

	/**
	 * \brief Returns tree path of a given node
	 * \param iter pointer to node whose path is to be retrieved
	 * \return pointer to path of node
	 */
	virtual GtkTreePath* getTreePath(GtkTreeIter* iter) const = 0;

	/**
	 * \brief Returns size_t value stored in the 'colType' column of node 'iter'
	 * \param iter pointer to node
	 * \param colType index of column where size_t value is stored
	 * \return size_t value retrieved at the specified column of the specified node, if any, 0 otherwise.
	 */
	virtual size_t getULongValueFromTreeIter(GtkTreeIter* iter, EVisualizationTreeColumn colType) const = 0;

	/**
	 * \brief Returns string stored in the 'colType' column of node 'iter'
	 * \param iter pointer to node
	 * \param string [out] string to be retrieved
	 * \param colType index of column where string is stored
	 * \return true if string was successfully retrieved at the specified column of the specified node, 0 otherwise.
	 */
	virtual bool getStringValueFromTreeIter(GtkTreeIter* iter, char*& string, EVisualizationTreeColumn colType) const = 0;

	/**
	 * \brief Returns pointer stored in the 'colType' column of node 'iter'
	 * \param iter pointer to node
	 * \param pointer [out] pointer to be retrieved
	 * \param colType index of column where pointer is stored
	 * \return true if pointer was successfully retrieved at the specified column of the specified node, 0 otherwise.
	 */
	virtual bool getPointerValueFromTreeIter(GtkTreeIter* iter, void*& pointer, EVisualizationTreeColumn colType) const = 0;

	/**
	 * \brief Returns identifier stored in the 'colType' column of node 'iter'
	 * \param iter pointer to node
	 * \param id [out] identifier to be retrieved
	 * \param colType index of column where identifier is stored
	 * \return true if identifier was successfully retrieved at the specified column of the specified node, 0 otherwise.
	 */
	virtual bool getIdentifierFromTreeIter(GtkTreeIter* iter, CIdentifier& id, EVisualizationTreeColumn colType) const = 0;
	//@}

	/**
	 * \name Node searching
	 */
	//@{

	/**
	 * \brief Looks for a node of name 'label' and type 'type' from the root of the internal tree store
	 * \param iter [out] pointer to node to be retrieved
	 * \param label label of node to be retrieved, as stored in the EVisualizationTreeColumn::StringName column
	 * \param type type of node to be retrieved, as stored in the EVisualizationTreeColumn::ULongNodtype column
	 * \return true if node was found, false otherwise
	 */
	virtual bool findChildNodeFromRoot(GtkTreeIter* iter, const char* label, EVisualizationTreeNode type) = 0;

	/**
	 * \brief Looks for a node of name 'label' and type 'type' from a given node in the internal tree store
	 * \param iter [in/out] pointer to node from which to start searching when calling function and to node found upon return
	 * \param label label of node to be retrieved, as stored in the EVisualizationTreeColumn::StringName column
	 * \param type type of node to be retrieved, as stored in the EVisualizationTreeColumn::ULongNodtype column
	 * \return true if node was found, false otherwise
	 */
	virtual bool findChildNodeFromParent(GtkTreeIter* iter, const char* label, EVisualizationTreeNode type) = 0;

	/**
	 * \brief Looks for a node whose associated Gtk widget matches 'widget' from the root of the internal tree store
	 * \param iter [out] pointer to node to be retrieved
	 * \param widget pointer to Gtk widget of node to be retrieved, as stored in the EVisualizationTreeColumn::PointerWidget column
	 * \return true if node was found, false otherwise
	 */
	virtual bool findChildNodeFromRoot(GtkTreeIter* iter, void* widget) = 0;

	/**
	 * \brief Looks for a node whose associated Gtk widget matches 'widget' from a given node in the internal tree store
	 * \param iter [in/out] pointer to node from which to start searching when calling function and to node found upon return
	 * \param widget pointer to Gtk widget of node to be retrieved, as stored in the EVisualizationTreeColumn::PointerWidget column
	 * \return true if node was found, false otherwise
	 */
	virtual bool findChildNodeFromParent(GtkTreeIter* iter, void* widget) = 0;

	/**
	 * \brief Looks for a node whose identifier matches 'identifier' from the root of the internal tree store
	 * \param iter [out] pointer to node to be retrieved
	 * \param id identifier of node to be retrieved, as stored in the EVisualizationTreeColumn::StringIdentifier column
	 * \return true if node was found, false otherwise
	 */
	virtual bool findChildNodeFromRoot(GtkTreeIter* iter, CIdentifier id) = 0;

	/**
	 * \brief Looks for a node whose identifier matches 'identifier' from a given node in the internal tree store
	 * \param iter [in/out] pointer to node from which to start searching when calling function and to node found upon return
	 * \param id identifier of node to be retrieved, as stored in the EVisualizationTreeColumn::StringIdentifier column
	 * \return true if node was found, false otherwise
	 */
	virtual bool findChildNodeFromParent(GtkTreeIter* iter, CIdentifier id) = 0;

	/**
	 * \brief Find first parent node of a given type
	 * Looks for first parent node of type 'type' from a given node in the internal tree store
	 * \param iter [in/out] pointer to node from which to start searching when calling function and to node found upon return
	 * \param type type of parent node looked for, as stored in the EVisualizationTreeColumn::ULongNodtype column
	 * \return true if node was found, false otherwise
	 */
	virtual bool findParentNode(GtkTreeIter* iter, EVisualizationTreeNode type) = 0;
	//@}

	/**
	 * \name Drag n' drop functions
	 */
	//@{

	/**
	 * \brief Handles drop of a widget in an existing widget
	 * This operation replaces the existing widget with the one passed in parameter. The existing widget
	 * is unaffected if it is of type EVisualizationTreeNode::VisualizationBox, or destroyed if it is of
	 * type EVisualizationTreeNode::Undefined (placeholder widget).
	 * \param srcWidgetID identifier of widget being dropped
	 * \param dstWidget pointer to widget on which the drop operation is performed
	 * \return true if drop operation was successfully completed, false otherwise
	 */
	virtual bool dragDataReceivedInWidgetCB(const CIdentifier& srcWidgetID, GtkWidget* dstWidget) = 0;

	/**
	 * \brief Handles drop of a widget in an event box
	 * This operation adds a widget to the tree and changes the tree structure.
	 * If a widget is moved from one place to the other in the tree, it is automatically
	 * simplified to avoid placeholders creation.
	 * \param srcWidgetID identifier of widget being dropped
	 * \param dstWidget pointer to widget on which the drop operation is performed
	 * \param location Position of drag
	 * \return true if drop operation was successfully completed, false otherwise
	 */
	virtual bool dragDataReceivedOutsideWidgetCB(const CIdentifier& srcWidgetID, GtkWidget* dstWidget, EDragLocation location) = 0;
	//@}

	/**
	 * \brief Forward pointer to the toolbar of a visualization plugin (if any) to the tree view
	 * \param boxID Identifier of IBox whose toolbar pointer is being set
	 * \param toolbar pointer to toolbar
	 * \return \e true in case of success, \e false otherwise.
	 */
	virtual bool setToolbar(const CIdentifier& boxID, GtkWidget* toolbar) = 0;

	/**
	 * \brief Forward pointer to the main widget of a visualization plugin to the tree view
	 * \param boxID Identifier of IBox whose topmost widget pointer is being set
	 * \param widget pointer to main window
	 * \return \e true in case of success, \e false otherwise.
	 */
	virtual bool setWidget(const CIdentifier& boxID, GtkWidget* widget) = 0;

	/**
	 * \return String representation of the visualization tree
	 */
	virtual CString serialize() const = 0;

	/**
	 * \brief Initializes the visualization tree from a string representation
	 * \param tree The string representation of the new tree
	 * \retval true In case of success
	 * \retval false In case of error
	 */
	virtual bool deserialize(const CString& tree) = 0;
};
}  // namespace VisualizationToolkit
}  // namespace OpenViBE
