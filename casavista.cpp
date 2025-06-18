#define NOMINMAX  // Prevent min/max macro warnings

#include "max.h"
#include "resource.h"
#include "simpmod.h"
#include "simpobj.h"
#include "iparamm2.h"
#include "mesh.h"
#include <iostream>
#include <vector>

// Helper function for debug output
void DebugOutput(const TCHAR* format, ...) {
    TCHAR buffer[1024];
    va_list args;
    va_start(args, format);
    _vstprintf_s(buffer, format, args);
    va_end(args);
    OutputDebugString(buffer);
}

// The unique Class_ID of this modifier
#define CASAVISTA_CID Class_ID(0x123456, 0x123456)

// This is the DLL instance handle passed in when the plug-in is loaded at startup
HINSTANCE hInstance;

// This function returns a pointer to a string in the string table
TCHAR* GetString(int id) {
    static TCHAR buf[256];
    if (hInstance)
        return LoadString(hInstance, id, buf, _countof(buf)) ? buf : NULL;
    return NULL;
}

// Parameter block indices
enum { casavista_params };

// Parameter indices
enum {
    casavista_class,
};

// Class names for the combo box
static const TCHAR* classNames[] = {
    _T("None"),
    _T("Interactable"),
    NULL
};

// Forward declarations for ALL Classes
class CasavistaMod;
class CasavistaClassDesc;
class CasavistaDlgProc;
class CasavistaPBAccessor;

// Forward declarations for dialog procedures
INT_PTR CALLBACK ModelSelectDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK MaterialSelectDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Forward declaration for helper function
void AddGeometryNodesToList(INode* node, HWND hList);
void UpdateModelsListBox(HWND hWnd, CasavistaMod* mod, INode* node);
void UpdateMaterialsListBox(HWND hWnd, CasavistaMod* mod, INode* node);

// Helper function to trim whitespace from a WStr
void TrimWStr(WStr& str) {
    const wchar_t* data = str.data();
    int len = str.Length();
    int start = 0;
    int end = len - 1;

    // Find start of non-whitespace
    while (start < len && (data[start] == L' ' || data[start] == L'\t' || data[start] == L'\n' || data[start] == L'\r')) {
        start++;
    }

    // Find end of non-whitespace
    while (end >= 0 && (data[end] == L' ' || data[end] == L'\t' || data[end] == L'\n' || data[end] == L'\r')) {
        end--;
    }

    if (start > 0 || end < len - 1) {
        str = str.Substr(start, end - start + 1);
    }
}

// 1. CasavistaMod Class Declaration
class CasavistaMod : public SimpleMod2 {
public:
    CasavistaMod();
    ~CasavistaMod();

    void DeleteThis() { delete this; }
    void GetClassName(MSTR& s, bool localized) const override;
    virtual Class_ID ClassID();
    void BeginEditParams(IObjParam* ip, ULONG flags, Animatable* prev);
    void EndEditParams(IObjParam* ip, ULONG flags, Animatable* next);
    IOResult Load(ILoad* iload);
    RefTargetHandle Clone(RemapDir& remap);
    const TCHAR* GetObjectName(bool localized) const override;

    Deformer& GetDeformer(TimeValue t, ModContext& mc, Matrix3& mat, Matrix3& invmat) override;
    Interval GetValidity(TimeValue t) override;
    void InvalidateUI() override;

    void SetClassProperty(INode* node, const TCHAR* className);
    const TCHAR* GetClassProperty(INode* node);

    // Override GetParamBlock to match base class
    IParamBlock2* GetParamBlock(int i) override { return pblock2; }
    int NumParamBlocks() override { return 1; }

    // New methods for models and materials
    void SetModelsProperty(INode* node, const TCHAR* models);
    const TCHAR* GetModelsProperty(INode* node);
    void SetMaterialsProperty(INode* node, const TCHAR* materials);
    const TCHAR* GetMaterialsProperty(INode* node);
    void ShowModelSelectDialog();
    void ShowMaterialSelectDialog();

    RefResult NotifyRefChanged(const Interval& changeInt, RefTargetHandle hTarget, PartID& partID, RefMessage message, BOOL propagate) override;

public:
    static IObjParam* ip; 
private:
    static CasavistaMod* editMod;
    IParamBlock2* pblock2;
    INode* editNode = nullptr; // Store the node pointer for property cleanup
};

// 2. CasavistaClassDesc Class Declaration
class CasavistaClassDesc : public ClassDesc2 {
public:
    int IsPublic() override;
    void* Create(BOOL loading = FALSE) override;
    const TCHAR* ClassName() override;
    SClass_ID SuperClassID() override;
    Class_ID ClassID() override;
    const TCHAR* Category() override;
    const TCHAR* InternalName() override;
    HINSTANCE HInstance() override;
    const TCHAR* NonLocalizedClassName() override;

    void BeginEditParams(IObjParam* ip, ReferenceMaker* vp, ULONG flags, Animatable* prev) override;
    void EndEditParams(IObjParam* ip, ReferenceMaker* vp, ULONG flags, Animatable* next) override;
};

// 3. CasavistaDlgProc Class Declaration
class CasavistaDlgProc : public ParamMap2UserDlgProc {
public:
    INT_PTR DlgProc(TimeValue t, IParamMap2* map, HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void DeleteThis();
};

// 4. CasavistaPBAccessor Class Declaration
class CasavistaPBAccessor : public PBAccessor
{
public:
    void Set(PB2Value& v, ReferenceMaker* owner, ParamID id, int tabIndex, TimeValue t) override;
};

// 5. Static Member Initializations for CasavistaMod
IObjParam* CasavistaMod::ip = NULL; 
CasavistaMod* CasavistaMod::editMod = NULL;

// 6. Global Instances (Declarations and Definitions)
static CasavistaClassDesc CasavistaDesc;
static CasavistaDlgProc theCasavistaProc;
static CasavistaPBAccessor theCasavistaPBAccessor;

// 7. Global Accessor Function Declaration
ClassDesc2* GetCasavistaDesc();

// 8. ParamBlockDesc2 casavista_param_blk Definition
ParamBlockDesc2 casavista_param_blk(
    casavista_params, _T("CasavistaParameters"), 0, &CasavistaDesc, P_AUTO_CONSTRUCT + P_AUTO_UI,
    SIMPMOD_PBLOCKREF,
    IDD_CASAVISTAPARAM, IDS_RB_PARAMETERS, 0, 0, &theCasavistaProc,
    casavista_class, _T("class"), TYPE_STRING, P_RESET_DEFAULT, IDS_RB_CLASS,
        p_default, _T("None"),
        p_accessor, &theCasavistaPBAccessor,
        p_nonLocalizedName, _T("Class"),
        p_end,
    p_end
);

// 9. Implementations of ALL Methods

// CasavistaMod Methods Implementations
CasavistaMod::CasavistaMod() : pblock2(NULL), editNode(nullptr) {
    CasavistaDesc.MakeAutoParamBlocks(this);
    assert(pblock2);

    // Initialize the parameter block with default value
    if (pblock2) {
        pblock2->SetValue(casavista_class, 0, _T("None"));
    }
}

CasavistaMod::~CasavistaMod() {
    // Clean up user properties if the modifier is being destroyed and node is still valid
    if (editNode) {
        editNode->SetUserPropBool(_T("HasCasavistaMod"), FALSE);
        editNode->SetUserPropString(_T("HasCasavistaMod"), NULL);
        editNode->SetUserPropString(_T("CasavistaClass"), NULL);
        editNode->SetUserPropString(_T("CasavistaModels"), NULL);
        editNode->SetUserPropString(_T("CasavistaMaterials"), NULL);
        editNode = nullptr;
    }
}

void CasavistaMod::GetClassName(MSTR& s, bool localized) const {
    s = localized ? GetString(IDS_RB_CASAVISTA_OSM_CLASS) : _T("Casavista Property Editor"); 
}

Class_ID CasavistaMod::ClassID() { 
    return CASAVISTA_CID; 
}

void CasavistaMod::BeginEditParams(IObjParam* ip, ULONG flags, Animatable* prev)
{
    this->ip = ip;
    editMod = this;

    SimpleMod2::BeginEditParams(ip, flags, prev);

    // When the modifier is first applied, initialize the user properties
    if (ip) {
        INode* node = ip->GetSelNode(0);
        editNode = node; // Store for later cleanup
        if (node) {
            // Check if properties already exist
            TSTR currentClass;
            TSTR currentModels;
            TSTR currentMaterials;
            BOOL hasMod = FALSE;

            node->GetUserPropString(_T("CasavistaClass"), currentClass);
            node->GetUserPropString(_T("CasavistaModels"), currentModels);
            node->GetUserPropString(_T("CasavistaMaterials"), currentMaterials);
            node->GetUserPropBool(_T("HasCasavistaMod"), hasMod);

            // Only initialize if properties don't exist
            if (!hasMod) {
                node->SetUserPropString(_T("CasavistaClass"), _T("None"));
                node->SetUserPropString(_T("CasavistaModels"), _T(""));
                node->SetUserPropString(_T("CasavistaMaterials"), _T(""));
                node->SetUserPropBool(_T("HasCasavistaMod"), TRUE);
            }
            else {
                // Update parameter block with existing values
                if (pblock2) {
                    pblock2->SetValue(casavista_class, 0, currentClass);
                }
            }
        }
    }

    CasavistaDesc.BeginEditParams(ip, this, flags, prev);
}

void CasavistaMod::EndEditParams(IObjParam* ip, ULONG flags, Animatable* next)
{
    // Save current state before ending edit
    if (ip) {
        INode* node = ip->GetSelNode(0);
        if (node && pblock2) {
            const TCHAR* className;
            Interval valid = FOREVER;
            pblock2->GetValue(casavista_class, 0, className, valid);
            node->SetUserPropString(_T("CasavistaClass"), className);
        }
    }

    editMod = NULL;
    CasavistaDesc.EndEditParams(ip, this, flags, next);
    SimpleMod2::EndEditParams(ip, flags, next);
    this->ip = NULL;
    this->editNode = nullptr;
}

IOResult CasavistaMod::Load(ILoad* iload)
{
    Modifier::Load(iload);
    return IO_OK;
}

RefTargetHandle CasavistaMod::Clone(RemapDir& remap)
{
    CasavistaMod* newmod = new CasavistaMod();
    newmod->SimpleMod2Clone(this, remap);
    BaseClone(this, newmod, remap);
    return newmod;
}

const TCHAR* CasavistaMod::GetObjectName(bool localized) const {
    return localized ? GetString(IDS_RB_CASAVISTA) : _T("Casavista Property Editor"); 
}

Deformer& CasavistaMod::GetDeformer(TimeValue t, ModContext& mc, Matrix3& mat, Matrix3& invmat)
{
    static Deformer nullDeformer;
    return nullDeformer;
}

Interval CasavistaMod::GetValidity(TimeValue t)
{
    Interval valid = FOREVER;
    return valid;
}

void CasavistaMod::InvalidateUI()
{
    if (pblock2) {
        casavista_param_blk.InvalidateUI(pblock2->LastNotifyParamID());
    }
}

void CasavistaMod::SetClassProperty(INode* node, const TCHAR* className)
{
    if (node) {
        // Only update if the value is different
        TSTR currentValue;
        node->GetUserPropString(_T("CasavistaClass"), currentValue);
        if (_tcscmp(currentValue, className) != 0) {
            node->SetUserPropString(_T("CasavistaClass"), className);
        }
    }
}

const TCHAR* CasavistaMod::GetClassProperty(INode* node)
{
    if (!node) {
        return _T("None");
    }

    static TSTR propValue;
    node->GetUserPropString(_T("CasavistaClass"), propValue);
    
    if (propValue.isNull() || propValue.Length() == 0) {
        // If property doesn't exist, initialize it
        node->SetUserPropString(_T("CasavistaClass"), _T("None"));
        return _T("None");
    }
    return propValue.data();
}

// New methods for models and materials
void CasavistaMod::SetModelsProperty(INode* node, const TCHAR* models)
{
    if (node) {
        node->SetUserPropString(_T("CasavistaModels"), models);
    }
}

const TCHAR* CasavistaMod::GetModelsProperty(INode* node)
{
    if (!node) {
        return _T("");
    }

    static TSTR propValue;
    node->GetUserPropString(_T("CasavistaModels"), propValue);
    
    if (propValue.isNull() || propValue.Length() == 0) {
        node->SetUserPropString(_T("CasavistaModels"), _T(""));
        return _T("");
    }
    return propValue.data();
}

void CasavistaMod::SetMaterialsProperty(INode* node, const TCHAR* materials)
{
    if (node) {
        node->SetUserPropString(_T("CasavistaMaterials"), materials);
    }
}

const TCHAR* CasavistaMod::GetMaterialsProperty(INode* node)
{
    if (!node) {
        return _T("");
    }

    static TSTR propValue;
    node->GetUserPropString(_T("CasavistaMaterials"), propValue);
    
    if (propValue.isNull() || propValue.Length() == 0) {
        node->SetUserPropString(_T("CasavistaMaterials"), _T(""));
        return _T("");
    }
    return propValue.data();
}

void CasavistaMod::ShowModelSelectDialog()
{
    if (!ip) return;

    // Create and show the model selection dialog
    DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_MODEL_SELECT), 
        ip->GetMAXHWnd(), (DLGPROC)ModelSelectDlgProc, (LPARAM)this);
}

void CasavistaMod::ShowMaterialSelectDialog()
{
    if (!ip) return;

    // Create and show the material selection dialog
    DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_MATERIAL_SELECT), 
        ip->GetMAXHWnd(), (DLGPROC)MaterialSelectDlgProc, (LPARAM)this);
}

RefResult CasavistaMod::NotifyRefChanged(const Interval& changeInt, RefTargetHandle hTarget, PartID& partID, RefMessage message, BOOL propagate) {
    if (message == REFMSG_TARGET_DELETED) {
        if (ip) {
            INode* node = ip->GetSelNode(0);
            if (node) {
                node->SetUserPropBool(_T("HasCasavistaMod"), FALSE);
                node->SetUserPropString(_T("HasCasavistaMod"), NULL);
                node->SetUserPropString(_T("CasavistaClass"), NULL);
                node->SetUserPropString(_T("CasavistaModels"), NULL);
                node->SetUserPropString(_T("CasavistaMaterials"), NULL);
            }
        }
    }
    return REF_SUCCEED;
}

// CasavistaClassDesc Methods Implementations
void* CasavistaClassDesc::Create(BOOL loading) {
    return new CasavistaMod();
}

const TCHAR* CasavistaClassDesc::ClassName() { return GetString(IDS_RB_CASAVISTA_OSM_CLASS); }
SClass_ID CasavistaClassDesc::SuperClassID() { return OSM_CLASS_ID; }
Class_ID CasavistaClassDesc::ClassID() { return CASAVISTA_CID; }
const TCHAR* CasavistaClassDesc::Category() { return GetString(IDS_MODBASED); }
const TCHAR* CasavistaClassDesc::InternalName() { return _T("CasavistaMod"); }
HINSTANCE CasavistaClassDesc::HInstance() { return hInstance; }
const TCHAR* CasavistaClassDesc::NonLocalizedClassName() { return _T("CasavistaMod"); }
int CasavistaClassDesc::IsPublic() { return 1; }

void CasavistaClassDesc::BeginEditParams(IObjParam* ip, ReferenceMaker* vp, ULONG flags, Animatable* prev)
{
    ClassDesc2::BeginEditParams(ip, vp, flags, prev);
}

void CasavistaClassDesc::EndEditParams(IObjParam* ip, ReferenceMaker* vp, ULONG flags, Animatable* next)
{
    ClassDesc2::EndEditParams(ip, vp, flags, next);
}

// CasavistaDlgProc Method Implementation
INT_PTR CasavistaDlgProc::DlgProc(TimeValue t, IParamMap2* map, HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    CasavistaMod* mod = (CasavistaMod*)map->GetParamBlock()->GetOwner();
    INode* node = NULL;
    if (mod && mod->ip) {
        node = mod->ip->GetSelNode(0);
    }

    switch (msg) {
    case WM_INITDIALOG:
        {
            SetWindowLongPtr(hWnd, GWLP_USERDATA, lParam);
            mod = (CasavistaMod*)lParam;

            // Get the node
            INode* node = mod->ip->GetSelNode(0);
            if (!node) return FALSE;

            // Initialize the class combo box
            HWND hCombo = GetDlgItem(hWnd, IDC_CLASS_COMBO);
            if (hCombo) {
                SendMessage(hCombo, CB_RESETCONTENT, 0, 0);
                SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)_T("None"));
                SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)_T("Interactable"));
                // Set selection based on user property
                TSTR classProp = mod->GetClassProperty(node);
                int selIndex = 0;
                if (!_tcsicmp(classProp, _T("Interactable"))) selIndex = 1;
                SendMessage(hCombo, CB_SETCURSEL, selIndex, 0);
            }

            // Initialize the models list
            UpdateModelsListBox(hWnd, mod, node);

            // Initialize the materials list
            UpdateMaterialsListBox(hWnd, mod, node);
        }
        return TRUE;

    case WM_COMMAND:
        {
            // Get the node
            INode* node = mod->ip->GetSelNode(0);
            if (!node) return FALSE;

            switch (LOWORD(wParam)) {
            case IDC_CLASS_COMBO:
                if (HIWORD(wParam) == CBN_SELCHANGE) {
                    HWND hCombo = GetDlgItem(hWnd, IDC_CLASS_COMBO);
                    if (hCombo) {
                        int index = SendMessage(hCombo, CB_GETCURSEL, 0, 0);
                        if (index != CB_ERR) {
                            TCHAR className[256] = { 0 };  // Initialize to zero
                            SendMessage(hCombo, CB_GETLBTEXT, index, (LPARAM)className);
                            mod->SetClassProperty(node, className);
                        }
                    }
                }
                break;

            case IDC_MODELS_ADD:
                if (mod) {
                    mod->ShowModelSelectDialog();
                    // Refresh the models list after dialog closes
                    INode* node = mod->ip->GetSelNode(0);
                    if (node) UpdateModelsListBox(hWnd, mod, node);
                }
                break;

            case IDC_MODELS_REMOVE:
                {
                    HWND hList = GetDlgItem(hWnd, IDC_MODELS_LIST);
                    if (hList) {
                        int selCount = SendMessage(hList, LB_GETSELCOUNT, 0, 0);
                        if (selCount > 0) {
                            std::vector<int> selItems(selCount);
                            SendMessage(hList, LB_GETSELITEMS, (WPARAM)selCount, (LPARAM)selItems.data());
                            std::sort(selItems.rbegin(), selItems.rend());
                            for (int i = 0; i < selCount; ++i) {
                                SendMessage(hList, LB_DELETESTRING, selItems[i], 0);
                            }
                            // Update the models property
                            WStr modelsStr;
                            int count = SendMessage(hList, LB_GETCOUNT, 0, 0);
                            for (int i = 0; i < count; i++) {
                                TCHAR modelName[256] = { 0 };
                                SendMessage(hList, LB_GETTEXT, i, (LPARAM)modelName);
                                if (i > 0) modelsStr += _T(",");
                                modelsStr += modelName;
                            }
                            mod->SetModelsProperty(node, modelsStr.data());
                            UpdateModelsListBox(hWnd, mod, node);
                        }
                    }
                }
                break;

            case IDC_MATERIALS_ADD:
                if (mod) {
                    mod->ShowMaterialSelectDialog();
                    INode* node = mod->ip->GetSelNode(0);
                    if (node) UpdateMaterialsListBox(hWnd, mod, node);
                }
                break;

            case IDC_MATERIALS_REMOVE:
                {
                    HWND hList = GetDlgItem(hWnd, IDC_MATERIALS_LIST);
                    if (hList) {
                        int selCount = SendMessage(hList, LB_GETSELCOUNT, 0, 0);
                        if (selCount > 0) {
                            std::vector<int> selItems(selCount);
                            SendMessage(hList, LB_GETSELITEMS, (WPARAM)selCount, (LPARAM)selItems.data());
                            std::sort(selItems.rbegin(), selItems.rend());
                            for (int i = 0; i < selCount; ++i) {
                                SendMessage(hList, LB_DELETESTRING, selItems[i], 0);
                            }
                            // Update the materials property
                            WStr materialsStr;
                            int count = SendMessage(hList, LB_GETCOUNT, 0, 0);
                            for (int i = 0; i < count; i++) {
                                TCHAR materialName[256] = { 0 };
                                SendMessage(hList, LB_GETTEXT, i, (LPARAM)materialName);
                                if (i > 0) materialsStr += _T(",");
                                materialsStr += materialName;
                            }
                            mod->SetMaterialsProperty(node, materialsStr.data());
                            UpdateMaterialsListBox(hWnd, mod, node);
                        }
                    }
                }
                break;
            }
        }
        return TRUE;

    case WM_DESTROY:
        return TRUE;
    }
    return FALSE;
}

void CasavistaDlgProc::DeleteThis() { /* Nothing to delete */ }

// CasavistaPBAccessor Method Implementation
void CasavistaPBAccessor::Set(PB2Value& v, ReferenceMaker* owner, ParamID id, int tabIndex, TimeValue t) 
{
    CasavistaMod* mod = (CasavistaMod*)owner;
    if (mod && mod->ip) { 
        INode* node = mod->ip->GetSelNode(0);
        if (node) {
            // Update both the user property and parameter block
            mod->SetClassProperty(node, v.s);
            
            // Force UI update
            if (mod->GetParamBlock(0)) {
                mod->GetParamBlock(0)->NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE);
            }
        }
    }
}

// GetCasavistaDesc Implementation
ClassDesc2* GetCasavistaDesc() { return &CasavistaDesc; }

// --- PlayerStartObject Implementation ---

#define PLAYERSTART_CID Class_ID(0x234567, 0x234567)

class PlayerStartObject : public SimpleObject2 {
public:
    bool propertySet;
    PlayerStartObject() : propertySet(false) {
        Interface* ip = GetCOREInterface();
        if (ip) {
            INode* node = ip->GetSelNode(0);
            if (node && node->GetObjectRef() == this) {
                node->SetUserPropString(_T("CasavistaClass"), _T("PlayerStart"));
                propertySet = true;
            }
        }
    }
    ~PlayerStartObject() {}
    void DeleteThis() override { delete this; }
    void GetClassName(MSTR& s, bool localized) const override { s = localized ? GetString(IDS_PLAYERSTART_OBJECT) : _T("Player Start"); }
    SClass_ID SuperClassID() override { return GEOMOBJECT_CLASS_ID; }
    Class_ID ClassID() override { return PLAYERSTART_CID; }
    const TCHAR* Category() { return GetString(IDS_CASAVISTA_CATEGORY); }
    const TCHAR* GetObjectName(bool localized) const override { return localized ? GetString(IDS_PLAYERSTART_OBJECT) : _T("Player Start"); }
    void BuildMesh(TimeValue t);
    void NotifyPostCreateNode(INode* node);
    CreateMouseCallBack* GetCreateMouseCallBack() override { return nullptr; }
    BOOL HasUVW() override { return FALSE; }
    void SetGenUVW(BOOL sw) override {}
};

class PlayerStartClassDesc : public ClassDesc2 {
public:
    int IsPublic() override { return 1; }
    void* Create(BOOL loading = FALSE) override { return new PlayerStartObject(); }
    const TCHAR* ClassName() override { return GetString(IDS_PLAYERSTART_OBJECT); }
    SClass_ID SuperClassID() override { return GEOMOBJECT_CLASS_ID; }
    Class_ID ClassID() override { return PLAYERSTART_CID; }
    const TCHAR* Category() { return GetString(IDS_CASAVISTA_CATEGORY); }
    const TCHAR* InternalName() override { return _T("PlayerStartObject"); }
    HINSTANCE HInstance() override { return hInstance; }
    const TCHAR* NonLocalizedClassName() override { return _T("PlayerStartObject"); }
};

static PlayerStartClassDesc playerStartDesc;
ClassDesc2* GetPlayerStartDesc() { return &playerStartDesc; }

// --- Mesh Building for Capsule + Arrow ---
void PlayerStartObject::BuildMesh(TimeValue t) {
    // Set user property on first mesh build
    if (!propertySet) {
        Interface* ip = GetCOREInterface();
        if (ip) {
            INode* node = ip->GetSelNode(0);
            if (node && node->GetObjectRef() == this) {
                node->SetUserPropString(_T("CasavistaClass"), _T("PlayerStart"));
                propertySet = true;
            }
        }
    }
    // Capsule parameters
    const float capsuleRadius = 45.0f; // cm
    const float capsuleHalfHeight = 90.0f; // cm
    const float capsuleHeight = capsuleHalfHeight * 2.0f;
    const int capsuleSides = 24;
    const int capsuleSegments = 8; // for hemispheres
    // Arrow parameters
    const float arrowLength = 60.0f; // cm
    const float arrowRadius = 8.0f; // cm
    const float coneLength = 18.0f; // cm
    const float coneRadius = 18.0f; // cm

    // Pivot at bottom: offset all Z by capsuleRadius
    const float zOffset = capsuleRadius;

    // --- Calculate mesh sizes ---
    int cylVerts = (capsuleSides + 1) * 2;
    int cylFaces = capsuleSides * 2;
    int hemiVerts = (capsuleSides + 1) * (capsuleSegments + 1);
    int hemiFaces = capsuleSides * capsuleSegments * 2; // 2 triangles per quad
    int arrowVerts = (capsuleSides + 1) * 2;
    int arrowFaces = capsuleSides * 2;
    int coneVerts = capsuleSides + 2;
    int coneFaces = capsuleSides;
    int totalVerts = cylVerts + hemiVerts * 2 + arrowVerts + coneVerts;
    int totalFaces = cylFaces + hemiFaces * 2 + arrowFaces + coneFaces;

    mesh.setNumVerts(totalVerts);
    mesh.setNumFaces(totalFaces);

    int v = 0, f = 0;

    // --- Capsule Cylinder (vertical, Z axis) ---
    float cylZ0 = zOffset;
    float cylZ1 = capsuleHeight + zOffset;
    for (int i = 0; i <= capsuleSides; ++i) {
        float angle = 2.0f * PI * float(i) / float(capsuleSides);
        float x = capsuleRadius * cosf(angle);
        float y = capsuleRadius * sinf(angle);
        mesh.setVert(v + i, Point3(x, y, cylZ0));
        mesh.setVert(v + i + capsuleSides + 1, Point3(x, y, cylZ1));
    }
    for (int i = 0; i < capsuleSides; ++i) {
        int i0 = v + i;
        int i1 = v + (i + 1);
        int i2 = v + i + capsuleSides + 1;
        int i3 = v + (i + 1) + capsuleSides + 1;
        mesh.faces[f].setVerts(i0, i1, i2);
        mesh.faces[f].setEdgeVisFlags(1, 1, 0);
        mesh.faces[f].setSmGroup(1);
        ++f;
        mesh.faces[f].setVerts(i1, i3, i2);
        mesh.faces[f].setEdgeVisFlags(1, 1, 0);
        mesh.faces[f].setSmGroup(1);
        ++f;
    }
    v += (capsuleSides + 1) * 2;
    if (v > totalVerts || f > totalFaces) DebugOutput(_T("ERROR: After cylinder v=%d/%d f=%d/%d\n"), v, totalVerts, f, totalFaces);
    DebugOutput(_T("After cylinder: v=%d, f=%d\n"), v, f);

    // --- Top Hemisphere (Z+) ---
    int topStart = v;
    float topCenterZ = capsuleHeight + zOffset;
    for (int y = 0; y <= capsuleSegments; ++y) {
        float phi = (PI / 2.0f) * (float(y) / float(capsuleSegments));
        float z = topCenterZ + capsuleRadius * sinf(phi);
        float r = capsuleRadius * cosf(phi);
        for (int i = 0; i <= capsuleSides; ++i) {
            float angle = 2.0f * PI * float(i) / float(capsuleSides);
            float x = r * cosf(angle);
            float y = r * sinf(angle);
            mesh.setVert(v++, Point3(x, y, z));
        }
    }
    for (int y = 0; y < capsuleSegments; ++y) {
        for (int i = 0; i < capsuleSides; ++i) {
            int row1 = topStart + y * (capsuleSides + 1);
            int row2 = topStart + (y + 1) * (capsuleSides + 1);
            mesh.faces[f].setVerts(row1 + i, row1 + i + 1, row2 + i);
            mesh.faces[f].setEdgeVisFlags(1, 1, 0);
            mesh.faces[f].setSmGroup(2);
            ++f;
            mesh.faces[f].setVerts(row1 + i + 1, row2 + i + 1, row2 + i);
            mesh.faces[f].setEdgeVisFlags(1, 1, 0);
            mesh.faces[f].setSmGroup(2);
            ++f;
        }
    }
    if (v > totalVerts || f > totalFaces) DebugOutput(_T("ERROR: After top hemi v=%d/%d f=%d/%d\n"), v, totalVerts, f, totalFaces);
    DebugOutput(_T("After top hemisphere: v=%d, f=%d\n"), v, f);

    // --- Bottom Hemisphere (Z-) ---
    int bottomStart = v;
    float bottomCenterZ = zOffset;
    for (int y = 0; y <= capsuleSegments; ++y) {
        float phi = (PI / 2.0f) * (float(y) / float(capsuleSegments));
        float z = bottomCenterZ - capsuleRadius * sinf(phi);
        float r = capsuleRadius * cosf(phi);
        for (int i = 0; i <= capsuleSides; ++i) {
            float angle = 2.0f * PI * float(i) / float(capsuleSides);
            float x = r * cosf(angle);
            float y = r * sinf(angle);
            mesh.setVert(v++, Point3(x, y, z));
        }
    }
    for (int y = 0; y < capsuleSegments; ++y) {
        for (int i = 0; i < capsuleSides; ++i) {
            int row1 = bottomStart + y * (capsuleSides + 1);
            int row2 = bottomStart + (y + 1) * (capsuleSides + 1);
            mesh.faces[f].setVerts(row1 + i, row2 + i, row1 + i + 1);
            mesh.faces[f].setEdgeVisFlags(1, 1, 0);
            mesh.faces[f].setSmGroup(3);
            ++f;
            mesh.faces[f].setVerts(row1 + i + 1, row2 + i, row2 + i + 1);
            mesh.faces[f].setEdgeVisFlags(1, 1, 0);
            mesh.faces[f].setSmGroup(3);
            ++f;
        }
    }
    if (v > totalVerts || f > totalFaces) DebugOutput(_T("ERROR: After bottom hemi v=%d/%d f=%d/%d\n"), v, totalVerts, f, totalFaces);
    DebugOutput(_T("After bottom hemisphere: v=%d, f=%d\n"), v, f);

    // --- Arrow shaft (cylinder along +Y, starts at capsule center) ---
    int arrowStart = v;
    float shaftY0 = 0.0f + 45.0f;
    float shaftY1 = arrowLength - coneLength + 45.0f;
    float arrowZ = capsuleHalfHeight + zOffset; // center of capsule
    for (int i = 0; i <= capsuleSides; ++i) {
        float angle = 2.0f * PI * float(i) / float(capsuleSides);
        float x = arrowRadius * cosf(angle);
        float z = arrowRadius * sinf(angle);
        mesh.setVert(v + i, Point3(x, shaftY0, arrowZ + z));
        mesh.setVert(v + i + capsuleSides + 1, Point3(x, shaftY1, arrowZ + z));
    }
    for (int i = 0; i < capsuleSides; ++i) {
        int i0 = arrowStart + i;
        int i1 = arrowStart + (i + 1);
        int i2 = arrowStart + i + capsuleSides + 1;
        int i3 = arrowStart + (i + 1) + capsuleSides + 1;
        mesh.faces[f].setVerts(i0, i1, i2);
        mesh.faces[f].setEdgeVisFlags(1, 1, 0);
        mesh.faces[f].setSmGroup(4);
        ++f;
        mesh.faces[f].setVerts(i1, i3, i2);
        mesh.faces[f].setEdgeVisFlags(1, 1, 0);
        mesh.faces[f].setSmGroup(4);
        ++f;
    }
    v += (capsuleSides + 1) * 2;
    if (v > totalVerts || f > totalFaces) DebugOutput(_T("ERROR: After arrow shaft v=%d/%d f=%d/%d\n"), v, totalVerts, f, totalFaces);
    DebugOutput(_T("After arrow shaft: v=%d, f=%d\n"), v, f);

    // --- Arrow head (cone, points +Y, starts at shaft end) ---
    int coneBase = v;
    mesh.setVert(v++, Point3(0.0f, arrowLength + 45.0f, arrowZ)); // tip
    for (int i = 0; i <= capsuleSides; ++i) {
        float angle = 2.0f * PI * float(i) / float(capsuleSides);
        float x = coneRadius * cosf(angle);
        float z = coneRadius * sinf(angle);
        mesh.setVert(v++, Point3(x, arrowLength - coneLength + 45.0f, arrowZ + z));
    }
    for (int i = 0; i < capsuleSides; ++i) {
        int tip = coneBase;
        int base0 = coneBase + 1 + i;
        int base1 = coneBase + 1 + ((i + 1) % (capsuleSides + 1));
        mesh.faces[f].setVerts(tip, base0, base1);
        mesh.faces[f].setEdgeVisFlags(1, 1, 0);
        mesh.faces[f].setSmGroup(5);
        ++f;
    }
    if (v > totalVerts || f > totalFaces) DebugOutput(_T("ERROR: After arrow cone v=%d/%d f=%d/%d\n"), v, totalVerts, f, totalFaces);
    DebugOutput(_T("After arrow cone: v=%d, f=%d\n"), v, f);

    mesh.InvalidateGeomCache();
    mesh.buildBoundingBox();
    DebugOutput(_T("PlayerStart mesh: %d verts, %d faces\n"), mesh.getNumVerts(), mesh.getNumFaces());
}

void PlayerStartObject::NotifyPostCreateNode(INode* node) {
    if (node) {
        node->SetUserPropString(_T("CasavistaClass"), _T("PlayerStart"));
    }
}

// --- DLL Exports Update ---
__declspec( dllexport ) int LibNumberClasses() { return 2; }
__declspec( dllexport ) ClassDesc *LibClassDesc(int i) {
    switch(i) {
        case 0: return GetCasavistaDesc();
        case 1: return GetPlayerStartDesc();
        default: return 0;
    }
}

// DLL entry point and Plugin exports
BOOL WINAPI DllMain(HINSTANCE hinstDLL, ULONG fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH) {
        hInstance = hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);
    }
    return TRUE;
}

__declspec(dllexport) const TCHAR* LibDescription() { return GetString(IDS_LIB_DESC); }
__declspec(dllexport) ULONG LibVersion() { return VERSION_3DSMAX; }

// Add these dialog procedures
INT_PTR CALLBACK ModelSelectDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    CasavistaMod* mod = (CasavistaMod*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    switch (msg) {
    case WM_INITDIALOG:
        {
            SetWindowLongPtr(hWnd, GWLP_USERDATA, lParam);
            mod = (CasavistaMod*)lParam;

            // Populate the list with available models
            HWND hList = GetDlgItem(hWnd, IDC_MODELS_LIST);
            if (hList && mod && mod->ip) {
                // Get the interface
                Interface* ip = mod->ip;
                
                // Get the root node
                INode* rootNode = ip->GetRootNode();
                if (rootNode) {
                    DebugOutput(_T("Starting to populate models list\n"));
                    // Add all geometry nodes to the list
                    AddGeometryNodesToList(rootNode, hList);
                    DebugOutput(_T("Finished populating models list\n"));
                }
            }
        }
        return TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            HWND hList = GetDlgItem(hWnd, IDC_MODELS_LIST);
            if (hList) {
                int selCount = SendMessage(hList, LB_GETSELCOUNT, 0, 0);
                if (selCount > 0) {
                    std::vector<int> selItems(selCount);
                    SendMessage(hList, LB_GETSELITEMS, (WPARAM)selCount, (LPARAM)selItems.data());
                    INode* node = mod->ip->GetSelNode(0);
                    if (node) {
                        TSTR currentModels = mod->GetModelsProperty(node);
                        for (int i = 0; i < selCount; ++i) {
                            TCHAR buffer[256];
                            SendMessage(hList, LB_GETTEXT, selItems[i], (LPARAM)buffer);
                            if (currentModels.Length() > 0) currentModels += _T(",");
                            currentModels += buffer;
                            // Also update the main dialog's list
                            HWND hMainList = GetDlgItem(GetParent(hWnd), IDC_MODELS_LIST);
                            if (hMainList) {
                                SendMessage(hMainList, LB_ADDSTRING, 0, (LPARAM)buffer);
                            }
                        }
                        mod->SetModelsProperty(node, currentModels);
                    }
                }
            }
            EndDialog(hWnd, IDOK);
            return TRUE;
        }
        else if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(hWnd, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

// Helper function to recursively add geometry nodes to the list
void AddGeometryNodesToList(INode* node, HWND hList)
{
    if (!node) return;

    TCHAR debugBuffer[1024];
    _stprintf_s(debugBuffer, _T("[Casavista] Processing node: %s\n"), node->GetName());
    OutputDebugString(debugBuffer);

    Object* obj = node->EvalWorldState(0).obj;
    _stprintf_s(debugBuffer, _T("[Casavista]   EvalWorldState: SuperClassID=%d, ClassID=(0x%08x, 0x%08x)\n"),
        obj ? obj->SuperClassID() : -1, obj ? obj->ClassID().PartA() : 0, obj ? obj->ClassID().PartB() : 0);
    OutputDebugString(debugBuffer);

    if (obj && obj->SuperClassID() == GEOMOBJECT_CLASS_ID) {
        _stprintf_s(debugBuffer, _T("[Casavista]   Adding geometry node: %s\n"), node->GetName());
        OutputDebugString(debugBuffer);
        SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)node->GetName());
    }

    // Process child nodes
    for (int i = 0; i < node->NumberOfChildren(); i++) {
        AddGeometryNodesToList(node->GetChildNode(i), hList);
    }
}

INT_PTR CALLBACK MaterialSelectDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    CasavistaMod* mod = (CasavistaMod*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    switch (msg) {
    case WM_INITDIALOG:
        {
            SetWindowLongPtr(hWnd, GWLP_USERDATA, lParam);
            mod = (CasavistaMod*)lParam;

            // Populate the list with available materials
            HWND hList = GetDlgItem(hWnd, IDC_MATERIALS_LIST);
            if (hList && mod && mod->ip) {
                // First get materials from the material editor
                const MtlBaseLib& editorMtlLib = mod->ip->GetMaterialLibrary();
                DebugOutput(_T("Material Editor Library Count: %d\n"), editorMtlLib.Count());
                
                // Add each material from the editor to the list
                for (int i = 0; i < editorMtlLib.Count(); i++) {
                    MtlBase* mtl = editorMtlLib[i];
                    if (mtl) {
                        MSTR name = mtl->GetName();
                        DebugOutput(_T("Adding editor material: %s\n"), name.data());
                        SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)name.data());
                    }
                }

                // Then get materials from the scene
                MtlBaseLib* sceneMtlLib = mod->ip->GetSceneMtls();
                DebugOutput(_T("Scene Material Library Count: %d\n"), sceneMtlLib ? sceneMtlLib->Count() : 0);
                
                // Add each material from the scene to the list if not already added
                if (sceneMtlLib) {
                    for (int i = 0; i < sceneMtlLib->Count(); i++) {
                        MtlBase* mtl = (*sceneMtlLib)[i];
                        if (mtl) {
                            MSTR name = mtl->GetName();
                            // Check if this material is already in the list
                            int count = SendMessage(hList, LB_GETCOUNT, 0, 0);
                            bool found = false;
                            for (int j = 0; j < count; j++) {
                                TCHAR existingName[256] = { 0 };
                                SendMessage(hList, LB_GETTEXT, j, (LPARAM)existingName);
                                existingName[255] = 0; // Ensure null-termination
                                if (_tcscmp(existingName, name.data()) == 0) {
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) {
                                DebugOutput(_T("Adding scene material: %s\n"), name.data());
                                SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)name.data());
                            }
                        }
                    }
                }
            }

            // Initialize the index spinner
            HWND hSpin = GetDlgItem(hWnd, IDC_MATERIAL_INDEX_SPIN);
            if (hSpin) {
                // Set the range (0 to 999)
                SendMessage(hSpin, UDM_SETRANGE32, 0, 999);
                // Set initial value to 0
                SendMessage(hSpin, UDM_SETPOS32, 0, 0);
                // Set buddy window
                SendMessage(hSpin, UDM_SETBUDDY, (WPARAM)GetDlgItem(hWnd, IDC_MATERIAL_INDEX_EDIT), 0);
            }

            // Initialize the edit box
            HWND hEdit = GetDlgItem(hWnd, IDC_MATERIAL_INDEX_EDIT);
            if (hEdit) {
                SetWindowText(hEdit, _T("0"));
            }
        }
        return TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            HWND hList = GetDlgItem(hWnd, IDC_MATERIALS_LIST);
            if (hList) {
                int selCount = SendMessage(hList, LB_GETSELCOUNT, 0, 0);
                if (selCount > 0) {
                    std::vector<int> selItems(selCount);
                    SendMessage(hList, LB_GETSELITEMS, (WPARAM)selCount, (LPARAM)selItems.data());
                    HWND hEdit = GetDlgItem(hWnd, IDC_MATERIAL_INDEX_EDIT);
                    int materialIndex = 0;
                    if (hEdit) {
                        TCHAR indexBuffer[32];
                        GetWindowText(hEdit, indexBuffer, 32);
                        materialIndex = _ttoi(indexBuffer);
                    }
                    INode* node = mod->ip->GetSelNode(0);
                    if (node) {
                        TSTR currentMaterials = mod->GetMaterialsProperty(node);
                        for (int i = 0; i < selCount; ++i) {
                            TCHAR buffer[256];
                            SendMessage(hList, LB_GETTEXT, selItems[i], (LPARAM)buffer);
                            if (currentMaterials.Length() > 0) currentMaterials += _T(",");
                            currentMaterials += buffer;
                            currentMaterials += _T(":");
                            TCHAR indexStr[32];
                            _stprintf_s(indexStr, _T("%d"), materialIndex);
                            currentMaterials += indexStr;
                            // Also update the main dialog's list
                            HWND hMainList = GetDlgItem(GetParent(hWnd), IDC_MATERIALS_LIST);
                            if (hMainList) {
                                TCHAR displayBuffer[512];
                                _stprintf_s(displayBuffer, _T("%s (Index: %d)"), buffer, materialIndex);
                                SendMessage(hMainList, LB_ADDSTRING, 0, (LPARAM)displayBuffer);
                            }
                        }
                        mod->SetMaterialsProperty(node, currentMaterials);
                    }
                }
            }
            EndDialog(hWnd, IDOK);
            return TRUE;
        }
        else if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(hWnd, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

// Helper to update the models list box
void UpdateModelsListBox(HWND hWnd, CasavistaMod* mod, INode* node) {
    HWND hModelsList = GetDlgItem(hWnd, IDC_MODELS_LIST);
    if (hModelsList) {
        SendMessage(hModelsList, LB_RESETCONTENT, 0, 0);
        const TCHAR* modelsStr = mod->GetModelsProperty(node);
        if (modelsStr && _tcslen(modelsStr) > 0) {
            WStr modelStr = modelsStr;
            int start = 0;
            int end = 0;
            while ((end = modelStr.first(',')) != -1) {
                WStr modelName = modelStr.Substr(start, end - start);
                while (modelName.Length() > 0 && modelName[0] == ' ') {
                    modelName = modelName.Substr(1, modelName.Length() - 1);
                }
                while (modelName.Length() > 0 && modelName[modelName.Length() - 1] == ' ') {
                    modelName = modelName.Substr(0, modelName.Length() - 1);
                }
                if (modelName.Length() > 0) {
                    SendMessage(hModelsList, LB_ADDSTRING, 0, (LPARAM)modelName.data());
                }
                start = end + 1;
                modelStr = modelStr.Substr(start, modelStr.Length() - start);
                start = 0;
            }
            if (modelStr.Length() > 0) {
                WStr modelName = modelStr;
                while (modelName.Length() > 0 && modelName[0] == ' ') {
                    modelName = modelName.Substr(1, modelName.Length() - 1);
                }
                while (modelName.Length() > 0 && modelName[modelName.Length() - 1] == ' ') {
                    modelName = modelName.Substr(0, modelName.Length() - 1);
                }
                if (modelName.Length() > 0) {
                    SendMessage(hModelsList, LB_ADDSTRING, 0, (LPARAM)modelName.data());
                }
            }
        }
    }
}

// Helper to update the materials list box
void UpdateMaterialsListBox(HWND hWnd, CasavistaMod* mod, INode* node) {
    HWND hMaterialsList = GetDlgItem(hWnd, IDC_MATERIALS_LIST);
    if (hMaterialsList) {
        SendMessage(hMaterialsList, LB_RESETCONTENT, 0, 0);
        const TCHAR* materialsStr = mod->GetMaterialsProperty(node);
        if (materialsStr && _tcslen(materialsStr) > 0) {
            WStr materialStr = materialsStr;
            int start = 0;
            int end = 0;
            while ((end = materialStr.first(',')) != -1) {
                WStr materialName = materialStr.Substr(start, end - start);
                while (materialName.Length() > 0 && materialName[0] == ' ') {
                    materialName = materialName.Substr(1, materialName.Length() - 1);
                }
                while (materialName.Length() > 0 && materialName[materialName.Length() - 1] == ' ') {
                    materialName = materialName.Substr(0, materialName.Length() - 1);
                }
                if (materialName.Length() > 0) {
                    SendMessage(hMaterialsList, LB_ADDSTRING, 0, (LPARAM)materialName.data());
                }
                start = end + 1;
                materialStr = materialStr.Substr(start, materialStr.Length() - start);
                start = 0;
            }
            if (materialStr.Length() > 0) {
                WStr materialName = materialStr;
                while (materialName.Length() > 0 && materialName[0] == ' ') {
                    materialName = materialName.Substr(1, materialName.Length() - 1);
                }
                while (materialName.Length() > 0 && materialName[materialName.Length() - 1] == ' ') {
                    materialName = materialName.Substr(0, materialName.Length() - 1);
                }
                if (materialName.Length() > 0) {
                    SendMessage(hMaterialsList, LB_ADDSTRING, 0, (LPARAM)materialName.data());
                }
            }
        }
    }
}