// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ActionInterface.hpp"
#include "Components.hpp"
#include "DataComponents.hpp"
#include "Dialogs/Message.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Engine/Waypoint/Waypoint.hpp"
#include "Engine/Waypoint/Waypoints.hpp"
#include "Form/Button.hpp"
#include "Interface.hpp"
#include "Language/Language.hpp"
#include "Look/DialogLook.hpp"
#include "MapSettings.hpp"
#include "Profile/Current.hpp"
#include "Profile/Profile.hpp"
#include "Profile/WaypointRingProfile.hpp"
#include "Renderer/DistanceRingsRenderer.hpp"
#include "Renderer/TextRowRenderer.hpp"
#include "UIGlobals.hpp"
#include "Units/Units.hpp"
#include "WaypointDialogs.hpp"
#include "Widget/ListWidget.hpp"
#include "Widget/RowFormWidget.hpp"

#include <cassert>

// The default waypoint ring radius, in meters
static constexpr double DEFAULT_RADIUS = 10000;

class RingDistanceWidget final : public RowFormWidget {
public:
  RingDistanceWidget(const DialogLook &look, double radius) noexcept
    : RowFormWidget{look}, radius{radius} {}

  double GetRadius() const noexcept {
    return radius;
  }

  void Prepare(ContainerWindow &parent,
               const PixelRect &rect) noexcept override {
    RowFormWidget::Prepare(parent, rect);
    AddFloat(_("Distance"), _("Radius of the ring around the waypoint."),
             "%.1f %s", "%.1f", 0.1, 1000, 1, true, UnitGroup::DISTANCE,
             radius);
  }

  bool Save(bool &changed) noexcept override {
    double new_radius = Units::ToSysDistance(GetValueFloat(0));
    if (new_radius != radius) {
      radius = new_radius;
      changed = true;
    }
    return true;
  }

private:
  double radius;
};

static bool
ShowRingDistanceDialog(const char *caption, double &radius)
{
  const DialogLook &look = UIGlobals::GetDialogLook();

  TWidgetDialog<RingDistanceWidget> dialog{WidgetDialog::Auto{},
                                           UIGlobals::GetMainWindow(),
                                           look, caption};
  dialog.AddButton(_("OK"), mrOK);
  dialog.AddButton(_("Cancel"), mrCancel);
  dialog.SetWidget(look, radius);

  if (dialog.ShowModal() != mrOK)
    return false;

  radius = dialog.GetWidget().GetRadius();
  return radius > 0;
}

static void
SaveWaypointRings() noexcept
{
  Profile::Save(Profile::map,
                CommonInterface::GetMapSettings().waypoint_rings);
  Profile::Save();
  ActionInterface::SendMapSettings(true);
}

bool
AddWaypointRing(const Waypoint &waypoint)
{
  auto &rings = CommonInterface::SetMapSettings().waypoint_rings;
  if (rings.IsFull()) {
    ShowMessageBox(_("Too many waypoint rings. Delete one first."),
                   C_("Setting", "Waypoint rings"),
                   MB_OK | MB_ICONEXCLAMATION);
    return false;
  }

  double radius = DEFAULT_RADIUS;
  if (!ShowRingDistanceDialog(waypoint.name.c_str(), radius) ||
      !rings.Add(waypoint.location, radius, waypoint.name.c_str()))
    return false;

  rings.enabled = true;
  SaveWaypointRings();
  return true;
}

class WaypointRingsWidget final : public ListWidget {
public:
  void CreateButtons(WidgetDialog &dialog);

  void Prepare(ContainerWindow &parent,
               const PixelRect &rect) noexcept override {
    const DialogLook &look = UIGlobals::GetDialogLook();
    CreateList(parent, look, rect,
               row_renderer.CalculateLayout(*look.list.font));
  }

  void Show(const PixelRect &rect) noexcept override {
    ListWidget::Show(rect);
    Update();
  }

private:
  static const WaypointRingSettings &GetRings() noexcept {
    return CommonInterface::GetMapSettings().waypoint_rings;
  }

  void Update() noexcept;

  void OnAddClicked();
  void OnEditClicked(unsigned i);
  void OnDeleteClicked(unsigned i) noexcept;

  void OnPaintItem(Canvas &canvas, const PixelRect rect,
                   unsigned idx) noexcept override;

  bool CanActivateItem(unsigned) const noexcept override { return true; }
  void OnActivateItem(unsigned index) noexcept override {
    OnEditClicked(index);
  }

private:
  Button *add_button = nullptr;
  Button *edit_button = nullptr;
  Button *delete_button = nullptr;

  TextRowRenderer row_renderer;
};

void
WaypointRingsWidget::CreateButtons(WidgetDialog &dialog)
{
  add_button = dialog.AddButton(C_("Button", "Add"), [this](){
    OnAddClicked();
  });

  edit_button = dialog.AddButton(_("Edit"), [this](){
    OnEditClicked(GetList().GetCursorIndex());
  });

  delete_button = dialog.AddButton(C_("Button", "Delete"), [this](){
    OnDeleteClicked(GetList().GetCursorIndex());
  });
}

void
WaypointRingsWidget::Update() noexcept
{
  const WaypointRingSettings &rings = GetRings();
  ListControl &list = GetList();
  list.SetLength(rings.count);
  list.Invalidate();

  add_button->SetEnabled(!rings.IsFull());
  edit_button->SetEnabled(!rings.IsEmpty());
  delete_button->SetEnabled(!rings.IsEmpty());
}

void
WaypointRingsWidget::OnAddClicked()
{
  auto waypoint = ShowWaypointListDialog(*data_components->waypoints,
                                         CommonInterface::Basic().location);
  if (!waypoint || !AddWaypointRing(*waypoint))
    return;

  Update();
  GetList().SetCursorIndex(GetRings().count - 1);
}

void
WaypointRingsWidget::OnEditClicked(unsigned i)
{
  if (i >= GetRings().count)
    return;

  auto &ring = CommonInterface::SetMapSettings().waypoint_rings.rings[i];
  double radius = ring.radius;
  if (!ShowRingDistanceDialog(ring.name, radius))
    return;

  ring.radius = radius;
  SaveWaypointRings();
  GetList().Invalidate();
}

void
WaypointRingsWidget::OnDeleteClicked(unsigned i) noexcept
{
  auto &rings = CommonInterface::SetMapSettings().waypoint_rings;
  if (i >= rings.count)
    return;

  rings.Remove(i);
  SaveWaypointRings();
  Update();
}

void
WaypointRingsWidget::OnPaintItem(Canvas &canvas, const PixelRect rect,
                                 unsigned i) noexcept
{
  const WaypointRingSettings &rings = GetRings();
  assert(i < rings.count);
  const WaypointRing &ring = rings.rings[i];

  char buffer[32];
  FormatRingDistance(ring.radius, buffer);

  PixelRect name_rect = rect;
  name_rect.right = row_renderer.DrawRightColumn(canvas, rect, buffer);
  row_renderer.DrawTextRow(canvas, name_rect, ring.name);
}

void
dlgWaypointRingsShowModal()
{
  const DialogLook &look = UIGlobals::GetDialogLook();

  TWidgetDialog<WaypointRingsWidget> dialog{
    WidgetDialog::Auto{}, UIGlobals::GetMainWindow(), look,
    C_("Setting", "Waypoint rings")};
  dialog.SetWidget();
  dialog.GetWidget().CreateButtons(dialog);
  dialog.AddButton(_("Close"), mrCancel);
  dialog.EnableCursorSelection();
  dialog.ShowModal();
}
