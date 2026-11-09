// ---- generated from Windows.UI.winmd (Windows.UI.Palettes) ----
namespace Pal {
struct IKnownPaletteKindsStatics;
struct IKnownPalettePropertiesStatics;
struct IPaletteCatalog;
struct IPaletteCatalogStatics;
struct IPaletteContent;
struct IPaletteContentFactory;
struct IPaletteContentView;
struct IPaletteContentViewFactory;
struct IPaletteDelegateInvoker;
struct IPaletteDelegateInvokerFactory;
struct IPaletteExtensionDefinition;
struct IPaletteExtensionHost;
struct IPaletteIconData;
struct IPaletteIconDataStatics;
struct IPaletteInvoker;
struct IPaletteInvokerFactory;
struct IPaletteItem;
struct IPaletteItemAnalytics;
struct IPaletteItemAnalyticsFactory;
struct IPaletteItemFactory;
struct IPaletteItemSourceExtension;
struct IPaletteItemView;
struct IPaletteKinds;
struct IPaletteKindsFactory;
struct IPaletteProgressState;
struct IPaletteProgressStateFactory;
struct IPaletteProgressStateView;
struct IPalettePropertyChangedEventArgs;
struct IPaletteQuery;
struct IPaletteQueryProgress;
struct IPaletteQueryProgressFactory;
struct IPaletteQueryResults;
struct IPaletteQueryResultsFactory;
struct IPaletteQueryResultsView;
struct IPaletteQueryStatics;
struct IPaletteStatus;
struct IPaletteStatusFactory;
struct IPaletteSystemInvoker;
struct IPaletteSystemInvokerFactory;
struct IPaletteTag;
struct IPaletteTagFactory;
struct IPaletteTemplateContent;
struct IPaletteTemplateContentFactory;
struct IPaletteTemplateContentView;
struct IPaletteToggleInvoker;
struct IPaletteToggleInvokerFactory;
struct IPaletteUriInvoker;
struct IPaletteUriInvokerFactory;
struct IVersionInfoStatics;
struct ExecutePaletteQueryDelegate;
struct IconResourceDelegate;
struct PaletteContentDelegate;
struct PaletteItemInvokeDelegate;
struct PaletteTemplateActionInvokeDelegate;
[[maybe_unused]] constexpr GUID IID_ExecutePaletteQueryDelegate = {0xD28C2DE0, 0x91DB, 0x55D2, {0xB3, 0x00, 0x27, 0x60, 0x4B, 0xAA, 0xC8, 0x19}};
struct ExecutePaletteQueryDelegate : IUnknown { virtual HRESULT STDMETHODCALLTYPE Invoke(Pal::IPaletteItemFactory* factory, Pal::IPaletteQuery* query, IInspectable** operation) = 0; };
[[maybe_unused]] constexpr GUID IID_IconResourceDelegate = {0x7BE9EF24, 0x7260, 0x5189, {0x83, 0x14, 0x1D, 0x9F, 0x3E, 0xF9, 0xC1, 0xE5}};
struct IconResourceDelegate : IUnknown { virtual HRESULT STDMETHODCALLTYPE Invoke(IInspectable* qualifierValues, IInspectable** result) = 0; };
[[maybe_unused]] constexpr GUID IID_PaletteContentDelegate = {0xAA523F7D, 0xE64D, 0x514F, {0x8F, 0xD7, 0x35, 0xD6, 0x5E, 0x86, 0x6E, 0xE7}};
struct PaletteContentDelegate : IUnknown { virtual HRESULT STDMETHODCALLTYPE Invoke(HSTRING itemId, IInspectable** operation) = 0; };
[[maybe_unused]] constexpr GUID IID_PaletteItemInvokeDelegate = {0xA2335A2E, 0xABC8, 0x5C1D, {0x8B, 0x62, 0x89, 0xC7, 0xF3, 0x9B, 0x0E, 0x0A}};
struct PaletteItemInvokeDelegate : IUnknown { virtual HRESULT STDMETHODCALLTYPE Invoke(Pal::IPaletteQuery* query) = 0; };
[[maybe_unused]] constexpr GUID IID_PaletteTemplateActionInvokeDelegate = {0xD3A63B88, 0xA52B, 0x59A7, {0x9E, 0x44, 0x50, 0xA5, 0x7A, 0x7F, 0x6E, 0xBF}};
struct PaletteTemplateActionInvokeDelegate : IUnknown { virtual HRESULT STDMETHODCALLTYPE Invoke(Pal::IPaletteTemplateContent* content, HSTRING actionId, HSTRING data, HSTRING inputs) = 0; };
[[maybe_unused]] constexpr GUID IID_IPaletteCatalog = {0x75D4E7E7, 0x9B3E, 0x577D, {0x9C, 0x34, 0x86, 0x76, 0x43, 0xFD, 0x71, 0x1A}};
struct IPaletteCatalog : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE GetPalette(HSTRING extensionId, Pal::IPaletteExtensionDefinition** result) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetPalettes(UINT32* __resultSize, Pal::IPaletteExtensionDefinition*** result) = 0;
  virtual HRESULT STDMETHODCALLTYPE add_Changed(IInspectable* handler, INT64* token) = 0;
  virtual HRESULT STDMETHODCALLTYPE remove_Changed(INT64 token) = 0;
};
[[maybe_unused]] constexpr GUID IID_IPaletteCatalogStatics = {0x084D2066, 0x3184, 0x50F3, {0xA0, 0xFF, 0x98, 0x04, 0xFF, 0x4C, 0x00, 0x02}};
struct IPaletteCatalogStatics : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE Parse(HSTRING input, UINT32* __resultSize, Pal::IPaletteItemView*** result) = 0;
  virtual HRESULT STDMETHODCALLTYPE Parse2(HSTRING input, Pal::IPaletteExtensionHost* host, UINT32* __resultSize, Pal::IPaletteItemView*** result) = 0;
  virtual HRESULT STDMETHODCALLTYPE TryParse(HSTRING input, UINT32* __itemsSize, Pal::IPaletteItemView*** items, boolean* succeeded) = 0;
  virtual HRESULT STDMETHODCALLTYPE TryParse2(HSTRING input, Pal::IPaletteExtensionHost* host, UINT32* __itemsSize, Pal::IPaletteItemView*** items, boolean* succeeded) = 0;
  virtual HRESULT STDMETHODCALLTYPE OpenDefault(Pal::IPaletteCatalog** result) = 0;
  virtual HRESULT STDMETHODCALLTYPE OpenWithKinds(Pal::IPaletteKinds* kindSpecs, Pal::IPaletteCatalog** result) = 0;
  virtual HRESULT STDMETHODCALLTYPE OpenWithKindsAndConfig(Pal::IPaletteKinds* kindSpecs, HSTRING configRegistryKeyPath, Pal::IPaletteCatalog** result) = 0;
  virtual HRESULT STDMETHODCALLTYPE IsCallerIdentityAccessSystemFor(IInspectable* calledObject, boolean* result) = 0;
  virtual HRESULT STDMETHODCALLTYPE TryGetItem(HSTRING extensionId, HSTRING itemId, Pal::IPaletteItemView** result) = 0;
};
[[maybe_unused]] constexpr GUID IID_IPaletteContent = {0x42CD7936, 0x3FDD, 0x59CF, {0xAF, 0x35, 0x55, 0x84, 0x42, 0x89, 0x12, 0x5F}};
struct IPaletteContent : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE get_Kind(INT32* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetView(Pal::IPaletteContentView** result) = 0;
};
[[maybe_unused]] constexpr GUID IID_IPaletteDelegateInvokerFactory = {0xE7898783, 0xCE65, 0x5218, {0xB5, 0x94, 0x9A, 0x6E, 0x9D, 0xBC, 0x5E, 0x6F}};
struct IPaletteDelegateInvokerFactory : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE CreateInstance(Pal::PaletteItemInvokeDelegate* handler, Pal::IPaletteDelegateInvoker** value) = 0;
};
[[maybe_unused]] constexpr GUID IID_IPaletteExtensionDefinition = {0xCA5CC86A, 0x8C7E, 0x5DB6, {0xB4, 0x6E, 0xD5, 0x88, 0xDE, 0xEE, 0x0A, 0xE7}};
struct IPaletteExtensionDefinition : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE CreateExtension(Pal::IPaletteExtensionHost* host, Pal::IPaletteItemSourceExtension** result) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_ItemFactory(Pal::IPaletteItemFactory** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_ExtensionId(HSTRING* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_PackageFamilyName(HSTRING* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_ApplicationId(HSTRING* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Icon(Pal::IPaletteIconData** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_DisplayName(HSTRING* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Description(HSTRING* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_AcceptedKindSpecs(UINT32* __valueSize, HSTRING** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Publisher(HSTRING* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Properties(IInspectable** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Kinds(Pal::IPaletteKinds** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_ContainedKinds(Pal::IPaletteKinds** value) = 0;
};
[[maybe_unused]] constexpr GUID IID_IPaletteExtensionHost = {0x3E789EFC, 0x49DD, 0x5670, {0x84, 0x58, 0x7F, 0xB5, 0x8C, 0x45, 0x72, 0x65}};
struct IPaletteExtensionHost : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE ReportStatusAsync(Pal::IPaletteStatus* status, IInspectable** operation) = 0;
  virtual HRESULT STDMETHODCALLTYPE ReportStatusAsyncWithMessage(HSTRING message, INT32 severity, IInspectable** operation) = 0;
  virtual HRESULT STDMETHODCALLTYPE ReportStatusAsyncWithProgress(HSTRING message, INT32 severity, Pal::IPaletteProgressState* progress, IInspectable** operation) = 0;
  virtual HRESULT STDMETHODCALLTYPE ReportStatusAsyncWithItem(HSTRING message, INT32 severity, Pal::IPaletteProgressState* progress, Pal::IPaletteItemView* item, IInspectable** operation) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetSystemInvoker(INT32 kind, Pal::IPaletteSystemInvoker** result) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetSystemInvokerWithContext(INT32 kind, IInspectable* context, Pal::IPaletteSystemInvoker** result) = 0;
  virtual HRESULT STDMETHODCALLTYPE add_Dismissed(IInspectable* handler, INT64* token) = 0;
  virtual HRESULT STDMETHODCALLTYPE remove_Dismissed(INT64 token) = 0;
  virtual HRESULT STDMETHODCALLTYPE RequestUnload() = 0;
};
[[maybe_unused]] constexpr GUID IID_IPaletteIconData = {0xA2C917A5, 0x6B93, 0x5D6D, {0xA3, 0xBB, 0x44, 0x3B, 0x82, 0xBE, 0x5B, 0x5C}};
struct IPaletteIconData : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE get_IconUri(IInspectable** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_IconGlyph(HSTRING* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_FontFamily(HSTRING* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_ImageStreamReference(IInspectable** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Bitmap(IInspectable** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetBitmapAsync(IInspectable* qualifierValues, IInspectable** operation) = 0;
};
[[maybe_unused]] constexpr GUID IID_IPaletteIconDataStatics = {0xBDE0245B, 0x9389, 0x565C, {0x8B, 0xF2, 0x09, 0x49, 0xA7, 0xBE, 0x7C, 0xD4}};
struct IPaletteIconDataStatics : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE FromResourceQualifier(Pal::IconResourceDelegate* handler, Pal::IPaletteIconData** result) = 0;
  virtual HRESULT STDMETHODCALLTYPE FromBitmap(IInspectable* bitmap, Pal::IPaletteIconData** result) = 0;
  virtual HRESULT STDMETHODCALLTYPE FromUri(IInspectable* iconUri, Pal::IPaletteIconData** result) = 0;
  virtual HRESULT STDMETHODCALLTYPE FromGlyph(HSTRING iconGlyph, Pal::IPaletteIconData** result) = 0;
  virtual HRESULT STDMETHODCALLTYPE FromGlyph2(HSTRING iconGlyph, HSTRING fontFamily, Pal::IPaletteIconData** result) = 0;
  virtual HRESULT STDMETHODCALLTYPE FromStreamReference(IInspectable* streamReference, Pal::IPaletteIconData** result) = 0;
};
[[maybe_unused]] constexpr GUID IID_IPaletteInvoker = {0x4DCE3641, 0xD67A, 0x5402, {0x82, 0xE6, 0xF7, 0x7B, 0xE4, 0x62, 0xC9, 0x42}};
struct IPaletteInvoker : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE Invoke(Pal::IPaletteQuery* query) = 0;
};
[[maybe_unused]] constexpr GUID IID_IPaletteItem = {0x095B1F54, 0x5206, 0x5F2B, {0xAC, 0xC5, 0xAE, 0x72, 0xBE, 0x80, 0x1E, 0x11}};
struct IPaletteItem : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE get_Id(HSTRING* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_ExtensionId(HSTRING* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Icon(Pal::IPaletteIconData** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_Icon(Pal::IPaletteIconData* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_IconOverlay(Pal::IPaletteIconData** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_IconOverlay(Pal::IPaletteIconData* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Title(HSTRING* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_Title(HSTRING value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Subtitle(UINT32* __valueSize, IInspectable*** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_Subtitle(UINT32 __valueSize, IInspectable** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Tags(UINT32* __valueSize, Pal::IPaletteTag*** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_Tags(UINT32 __valueSize, Pal::IPaletteTag** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Properties(IInspectable** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetView(Pal::IPaletteItemView** result) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Status(Pal::IPaletteStatus** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_Status(Pal::IPaletteStatus* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Kinds(Pal::IPaletteKinds** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Invoker(Pal::IPaletteInvoker** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_Invoker(Pal::IPaletteInvoker* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_ExecuteQueryDelegate(Pal::ExecutePaletteQueryDelegate** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_ExecuteQueryDelegate(Pal::ExecutePaletteQueryDelegate* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_ContentDelegate(Pal::PaletteContentDelegate** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_ContentDelegate(Pal::PaletteContentDelegate* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE add_PropertyChanged(IInspectable* handler, INT64* token) = 0;
  virtual HRESULT STDMETHODCALLTYPE remove_PropertyChanged(INT64 token) = 0;
};
[[maybe_unused]] constexpr GUID IID_IPaletteItemFactory = {0x72784ABE, 0xDA8A, 0x5F48, {0x89, 0x5C, 0xD8, 0xE3, 0x1D, 0x03, 0xAE, 0x40}};
struct IPaletteItemFactory : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE get_ExtensionId(HSTRING* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE Create(HSTRING id, Pal::IPaletteItem** result) = 0;
  virtual HRESULT STDMETHODCALLTYPE Create2(HSTRING id, Pal::IPaletteKinds* kinds, Pal::IPaletteItem** result) = 0;
};
[[maybe_unused]] constexpr GUID IID_IPaletteItemSourceExtension = {0x2F89BC97, 0x3238, 0x5E81, {0x8E, 0x02, 0x5D, 0xAA, 0x9C, 0x56, 0x3F, 0x81}};
struct IPaletteItemSourceExtension : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE Initialize(Pal::IPaletteItemFactory* factory, Pal::IPaletteExtensionHost* host) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetItem(HSTRING id, Pal::IPaletteItemView** result) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetItems(UINT32* __resultSize, Pal::IPaletteItemView*** result) = 0;
  virtual HRESULT STDMETHODCALLTYPE ExecuteQueryAsync(Pal::IPaletteQuery* query, IInspectable** operation) = 0;
  virtual HRESULT STDMETHODCALLTYPE ReportItemAnalyticsAsync(UINT32 __analyticsSize, Pal::IPaletteItemAnalytics** analytics, IInspectable** operation) = 0;
  virtual HRESULT STDMETHODCALLTYPE ReportConversationAnalyticsAsync(UINT32 __itemAnalyticsSize, Pal::IPaletteItemAnalytics** itemAnalytics, IInspectable** operation) = 0;
  virtual HRESULT STDMETHODCALLTYPE add_ItemsChanged(IInspectable* handler, INT64* token) = 0;
  virtual HRESULT STDMETHODCALLTYPE remove_ItemsChanged(INT64 token) = 0;
};
[[maybe_unused]] constexpr GUID IID_IPaletteItemView = {0x49D58C99, 0x5BBF, 0x5745, {0xA6, 0x93, 0x67, 0xC1, 0x67, 0x15, 0xF0, 0xC0}};
struct IPaletteItemView : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE get_Id(HSTRING* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_ExtensionId(HSTRING* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Icon(Pal::IPaletteIconData** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_IconOverlay(Pal::IPaletteIconData** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Title(HSTRING* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Subtitle(UINT32* __valueSize, IInspectable*** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Tags(UINT32* __valueSize, Pal::IPaletteTag*** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Properties(IInspectable** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE ExecuteQueryAsync(Pal::IPaletteQuery* query, IInspectable** operation) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Status(Pal::IPaletteStatus** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_IsContentAvailable(boolean* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetContentAsync(IInspectable** operation) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Kinds(Pal::IPaletteKinds** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE add_ContentChanged(IInspectable* handler, INT64* token) = 0;
  virtual HRESULT STDMETHODCALLTYPE remove_ContentChanged(INT64 token) = 0;
  virtual HRESULT STDMETHODCALLTYPE TryGetInvoker(Pal::IPaletteInvoker** result) = 0;
  virtual HRESULT STDMETHODCALLTYPE add_PropertyChanged(IInspectable* handler, INT64* token) = 0;
  virtual HRESULT STDMETHODCALLTYPE remove_PropertyChanged(INT64 token) = 0;
};
[[maybe_unused]] constexpr GUID IID_IPaletteKinds = {0x666CB275, 0x1764, 0x5016, {0xA7, 0x30, 0x1B, 0xF7, 0xE9, 0xE1, 0xD1, 0xBC}};
struct IPaletteKinds : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE get_Values(UINT32* __valueSize, HSTRING** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetPrimaryKind(HSTRING* result) = 0;
  virtual HRESULT STDMETHODCALLTYPE IsOfKind(HSTRING kindSpec, boolean* result) = 0;
  virtual HRESULT STDMETHODCALLTYPE IsOfKinds(UINT32 __kindSpecsSize, HSTRING* kindSpecs, INT32 selector, boolean* result) = 0;
};
[[maybe_unused]] constexpr GUID IID_IPaletteKindsFactory = {0xBE554067, 0x0C61, 0x5273, {0xB6, 0x6E, 0xDC, 0x14, 0x2F, 0x6E, 0x01, 0xA9}};
struct IPaletteKindsFactory : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE CreateInstance(UINT32 __valuesSize, HSTRING* values, Pal::IPaletteKinds** value) = 0;
};
[[maybe_unused]] constexpr GUID IID_IPaletteProgressStateFactory = {0xDBC1B51F, 0x0FD4, 0x5C73, {0x9F, 0x3E, 0xD1, 0xDE, 0x7E, 0x18, 0x5B, 0x72}};
struct IPaletteProgressStateFactory : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE CreateInstance(boolean isIndeterminate, FLOAT progress, Pal::IPaletteProgressState** value) = 0;
};
[[maybe_unused]] constexpr GUID IID_IPaletteQuery = {0xA3D9EE47, 0x0151, 0x5A33, {0x96, 0x4A, 0xD2, 0xD4, 0xC9, 0x1E, 0x97, 0xEA}};
struct IPaletteQuery : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE ToJson(HSTRING* result) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_QueryId(GUID* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Arguments(UINT32* __valueSize, IInspectable*** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_Arguments(UINT32 __valueSize, IInspectable** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_DisplayLanguageTag(HSTRING* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_DisplayLanguageTag(HSTRING value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_SystemLanguageTag(HSTRING* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_SystemLanguageTag(HSTRING value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_ItemCount(INT32* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_ItemCount(INT32 value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Scopes(UINT32* __valueSize, Pal::IPaletteItemView*** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_Scopes(UINT32 __valueSize, Pal::IPaletteItemView** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_RequestedProperties(UINT32* __valueSize, HSTRING** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_RequestedProperties(UINT32 __valueSize, HSTRING* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Context(IInspectable** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Mode(INT32* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_Mode(INT32 value) = 0;
};
[[maybe_unused]] constexpr GUID IID_IPaletteQueryProgressFactory = {0x16412CF9, 0x3F5E, 0x5E6C, {0x85, 0xCD, 0x8F, 0xE5, 0x7F, 0x1B, 0xC5, 0x38}};
struct IPaletteQueryProgressFactory : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE CreateInstance(UINT32 __itemsSize, Pal::IPaletteItemView** items, Pal::IPaletteProgressState* progressState, Pal::IPaletteQueryProgress** value) = 0;
};
[[maybe_unused]] constexpr GUID IID_IPaletteQueryResults = {0x587B3BED, 0x2C37, 0x53AF, {0xB4, 0x79, 0xC2, 0x98, 0x6B, 0xCA, 0x59, 0xDC}};
struct IPaletteQueryResults : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE get_QueryId(GUID* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Items(UINT32* __valueSize, Pal::IPaletteItemView*** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_Items(UINT32 __valueSize, Pal::IPaletteItemView** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_HasMoreItems(boolean* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_HasMoreItems(boolean value) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetView(Pal::IPaletteQueryResultsView** result) = 0;
};
[[maybe_unused]] constexpr GUID IID_IPaletteQueryResultsFactory = {0x3C262865, 0xA9E4, 0x5C17, {0xB3, 0x03, 0xEC, 0xC3, 0x10, 0xF5, 0xEB, 0x71}};
struct IPaletteQueryResultsFactory : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE CreateInstance(Pal::IPaletteQuery* query, Pal::IPaletteQueryResults** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateInstance2(Pal::IPaletteQuery* query, Pal::ExecutePaletteQueryDelegate* getNextDelegate, Pal::IPaletteQueryResults** value) = 0;
};
[[maybe_unused]] constexpr GUID IID_IPaletteQueryResultsView = {0xE11F2FF3, 0x5F5B, 0x503F, {0x84, 0x4F, 0xFD, 0x2B, 0xB9, 0xF0, 0x63, 0x3D}};
struct IPaletteQueryResultsView : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE get_QueryId(GUID* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Items(UINT32* __valueSize, Pal::IPaletteItemView*** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_HasMoreItems(boolean* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetMoreItemsAsync(IInspectable** operation) = 0;
};
[[maybe_unused]] constexpr GUID IID_IPaletteTag = {0xEF42C037, 0xBC6B, 0x5802, {0xA6, 0xAE, 0xD4, 0xB4, 0x33, 0x2A, 0xBD, 0x32}};
struct IPaletteTag : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE get_Icon(Pal::IPaletteIconData** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Text(HSTRING* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_ToolTip(HSTRING* value) = 0;
};
[[maybe_unused]] constexpr GUID IID_IPaletteTagFactory = {0x64702731, 0x160E, 0x580F, {0xAD, 0xD9, 0xC6, 0xA8, 0xB7, 0x78, 0x86, 0xC3}};
struct IPaletteTagFactory : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE CreateInstance(Pal::IPaletteIconData* icon, HSTRING text, HSTRING tooltip, Pal::IPaletteTag** value) = 0;
};
[[maybe_unused]] constexpr GUID IID_IPaletteTemplateContent = {0xA2561B14, 0x488B, 0x5B78, {0xAC, 0x4B, 0x00, 0x13, 0x9C, 0xCE, 0x94, 0x99}};
struct IPaletteTemplateContent : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE get_TemplateJson(HSTRING* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_TemplateJson(HSTRING value) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_DataJson(HSTRING* value) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_DataJson(HSTRING value) = 0;
  virtual HRESULT STDMETHODCALLTYPE add_PropertyChanged(IInspectable* handler, INT64* token) = 0;
  virtual HRESULT STDMETHODCALLTYPE remove_PropertyChanged(INT64 token) = 0;
};
[[maybe_unused]] constexpr GUID IID_IPaletteTemplateContentFactory = {0x1AFD3A97, 0x0151, 0x5E70, {0xA9, 0x6A, 0xFF, 0xD7, 0x14, 0xE9, 0xD8, 0x2D}};
struct IPaletteTemplateContentFactory : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE CreateInstance(Pal::PaletteTemplateActionInvokeDelegate* invokeAction, Pal::IPaletteTemplateContent** value) = 0;
};
[[maybe_unused]] constexpr GUID IID_IPaletteToggleInvokerFactory = {0x48CFA05A, 0x6C07, 0x5DEE, {0xA3, 0x63, 0x1F, 0x5A, 0x48, 0xBB, 0x5A, 0x8B}};
struct IPaletteToggleInvokerFactory : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE CreateInstance(Pal::IPaletteInvoker* invoker, boolean isToggled, Pal::IPaletteToggleInvoker** value) = 0;
};
[[maybe_unused]] constexpr GUID IID_IPaletteUriInvokerFactory = {0x8F2D6F1B, 0x4B0D, 0x5D77, {0xA4, 0x65, 0x6F, 0xD9, 0x67, 0x31, 0x70, 0x9D}};
struct IPaletteUriInvokerFactory : IInspectable {
  virtual HRESULT STDMETHODCALLTYPE CreateInstance(IInspectable* uri, Pal::IPaletteUriInvoker** value) = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateInstance2(IInspectable* uri, IInspectable* launcherOptions, Pal::IPaletteUriInvoker** value) = 0;
};
} // namespace Pal
