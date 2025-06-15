/*===========================================================================*\
  Casavista Property Editor Modifier

  FILE: casavista.cpp

  DESCRIPTION:  Property Editor for Casavista objects

  CREATED BY: Claude

  HISTORY: created 2024

  Copyright (c) 2024, All Rights Reserved.
\*===========================================================================*/

#include "max.h"
#include "resource.h"
#include "simpmod.h"
#include "simpobj.h"
#include "iparamm2.h"

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

public:
    static IObjParam* ip; 
private:
    static CasavistaMod* editMod;
    IParamBlock2* pblock2;
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
CasavistaMod::CasavistaMod() : pblock2(NULL) {
    CasavistaDesc.MakeAutoParamBlocks(this);
    assert(pblock2);

    // Initialize the parameter block with default value
    if (pblock2) {
        pblock2->SetValue(casavista_class, 0, _T("None"));
    }
}

CasavistaMod::~CasavistaMod() {
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
        if (node) {
            // Clear any existing properties first
            node->SetUserPropString(_T("CasavistaClass"), NULL);
            
            // Set initial values
            node->SetUserPropString(_T("CasavistaClass"), _T("None"));
            node->SetUserPropBool(_T("HasCasavistaMod"), TRUE);
        }
    }

    CasavistaDesc.BeginEditParams(ip, this, flags, prev);
}

void CasavistaMod::EndEditParams(IObjParam* ip, ULONG flags, Animatable* next)
{
    editMod = NULL;

    CasavistaDesc.EndEditParams(ip, this, flags, next);

    SimpleMod2::EndEditParams(ip, flags, next);
    this->ip = NULL;
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
            HWND hCombo = GetDlgItem(hWnd, IDC_CLASS_COMBO);
            if (hCombo) {
                SendMessage(hCombo, CB_RESETCONTENT, 0, 0);
                
                for (int i = 0; classNames[i] != NULL; i++) {
                    SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)classNames[i]);
                }

                const TCHAR* currentClass = _T("None");
                if (mod && node) {
                    currentClass = mod->GetClassProperty(node);
                }

                int index = 0;
                if (currentClass) {
                    for (int i = 0; classNames[i] != NULL; i++) {
                        if (_tcscmp(currentClass, classNames[i]) == 0) {
                            index = i;
                            break;
                        }
                    }
                }
                SendMessage(hCombo, CB_SETCURSEL, index, 0);
            }
        }
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_CLASS_COMBO && HIWORD(wParam) == CBN_SELCHANGE) {
            HWND hCombo = GetDlgItem(hWnd, IDC_CLASS_COMBO);
            if (hCombo) {
                int index = SendMessage(hCombo, CB_GETCURSEL, 0, 0);
                if (index >= 0) {
                    TCHAR buffer[256];
                    SendMessage(hCombo, CB_GETLBTEXT, index, (LPARAM)buffer);
                    
                    // Update both the parameter block and user property
                    if (mod && node) {
                        mod->SetClassProperty(node, buffer);
                        if (mod->GetParamBlock(0)) {
                            mod->GetParamBlock(0)->SetValue(casavista_class, t, buffer);
                        }
                    }
                }
            }
            return TRUE;
        }
        break;
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
__declspec(dllexport) int LibNumberClasses() { return 1; }
__declspec(dllexport) ClassDesc* LibClassDesc(int i) { return GetCasavistaDesc(); }
__declspec(dllexport) ULONG LibVersion() { return VERSION_3DSMAX; }