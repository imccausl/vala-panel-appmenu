#include "unitygtkmenu.h"

#define UNITY_GTK_TYPE_MENU_ITEM                (unity_gtk_menu_item_get_type ())
#define UNITY_GTK_MENU_ITEM(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), UNITY_GTK_TYPE_MENU_ITEM, UnityGtkMenuItem))
#define UNITY_GTK_IS_MENU_ITEM(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), UNITY_GTK_TYPE_MENU_ITEM))
#define UNITY_GTK_MENU_ITEM_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), UNITY_GTK_TYPE_MENU_ITEM, UnityGtkMenuItemClass))
#define UNITY_GTK_IS_MENU_ITEM_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), UNITY_GTK_TYPE_MENU_ITEM))
#define UNITY_GTK_MENU_ITEM_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), UNITY_GTK_TYPE_MENU_ITEM, UnityGtkMenuItemClass))
#define UNITY_GTK_TYPE_MENU_SECTION             (unity_gtk_menu_section_get_type ())
#define UNITY_GTK_MENU_SECTION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), UNITY_GTK_TYPE_MENU_SECTION, UnityGtkMenuSection))
#define UNITY_GTK_IS_MENU_SECTION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), UNITY_GTK_TYPE_MENU_SECTION))
#define UNITY_GTK_MENU_SECTION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), UNITY_GTK_TYPE_MENU_SECTION, UnityGtkMenuSectionClass))
#define UNITY_GTK_IS_MENU_SECTION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), UNITY_GTK_TYPE_MENU_SECTION))
#define UNITY_GTK_MENU_SECTION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), UNITY_GTK_TYPE_MENU_SECTION, UnityGtkMenuSectionClass))
#define UNITY_GTK_MENU_GET_PRIVATE(obj)         (G_TYPE_INSTANCE_GET_PRIVATE ((obj), UNITY_GTK_TYPE_MENU, UnityGtkMenuPrivate))
#define UNITY_GTK_ACTION_GROUP_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), UNITY_GTK_TYPE_ACTION_GROUP, UnityGtkActionGroupPrivate))

typedef struct _UnityGtkMenuItem         UnityGtkMenuItem;
typedef struct _UnityGtkMenuItemClass    UnityGtkMenuItemClass;
typedef struct _UnityGtkMenuSection      UnityGtkMenuSection;
typedef struct _UnityGtkMenuSectionClass UnityGtkMenuSectionClass;

struct _UnityGtkMenuItem
{
  GObject parent_instance;

  /*< private >*/
  GtkMenuItem         *menu_item;
  UnityGtkMenuSection *parent_section;
  UnityGtkMenu        *submenu;
  guint                submenu_valid : 1;
  gulong               menu_item_notify_handler_id;
};

struct _UnityGtkMenuItemClass
{
  GObjectClass parent_class;
};

/* The last item may be a separator. */
struct _UnityGtkMenuSection
{
  GMenuModel parent_instance;

  /*< private >*/
  UnityGtkMenu *parent_menu;
  guint         shell_offset;
  GPtrArray    *items;
};

struct _UnityGtkMenuSectionClass
{
  GMenuModelClass parent_class;
};

struct _UnityGtkMenuPrivate
{
  GtkMenuShell        *menu_shell;
  GPtrArray           *sections;
  UnityGtkActionGroup *action_group;
  gulong               menu_shell_insert_handler_id;
};

struct _UnityGtkActionGroupPrivate
{
};

static void unity_gtk_action_group_g_action_group_init (GActionGroupInterface *iface);

G_DEFINE_TYPE (UnityGtkMenuItem, unity_gtk_menu_item, G_TYPE_OBJECT);
G_DEFINE_TYPE (UnityGtkMenuSection, unity_gtk_menu_section, G_TYPE_MENU_MODEL);
G_DEFINE_TYPE (UnityGtkMenu, unity_gtk_menu, G_TYPE_MENU_MODEL);
G_DEFINE_TYPE_WITH_CODE (UnityGtkActionGroup, unity_gtk_action_group, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, unity_gtk_action_group_g_action_group_init));

enum
{
  MENU_ITEM_PROP_0,
  MENU_ITEM_PROP_MENU_ITEM,
  MENU_ITEM_PROP_PARENT_SECTION,
  MENU_ITEM_PROP_SUBMENU,
  MENU_ITEM_N_PROPERTIES
};

enum
{
  MENU_SECTION_PROP_0,
  MENU_SECTION_PROP_PARENT_MENU,
  MENU_SECTION_PROP_SHELL_OFFSET,
  MENU_SECTION_PROP_ITEMS,
  MENU_SECTION_N_PROPERTIES
};

enum
{
  MENU_PROP_0,
  MENU_PROP_MENU_SHELL,
  MENU_PROP_SECTIONS,
  MENU_PROP_ACTION_GROUP,
  MENU_N_PROPERTIES
};

static GParamSpec *menu_item_properties[MENU_ITEM_N_PROPERTIES] = { NULL };
static GParamSpec *menu_section_properties[MENU_SECTION_N_PROPERTIES] = { NULL };
static GParamSpec *menu_properties[MENU_N_PROPERTIES] = { NULL };

static void unity_gtk_menu_remove_item (UnityGtkMenu     *menu,
                                        UnityGtkMenuItem *item);

static gint
g_ptr_array_find (GPtrArray *ptr_array,
                  gpointer   data)
{
  guint i;

  g_return_val_if_fail (ptr_array != NULL, -1);

  for (i = 0; i < ptr_array->len; i++)
    if (g_ptr_array_index (ptr_array, i) == data)
      return i;

  return -1;
}

static void
g_ptr_array_insert (GPtrArray *ptr_array,
                    gpointer   data,
                    gint       index)
{
  guint i;

  g_return_if_fail (ptr_array != NULL);
  g_warn_if_fail (-1 <= index && index <= ptr_array->len);

  if (index < 0 || index > ptr_array->len)
    index = ptr_array->len;

  g_ptr_array_add (ptr_array, NULL);

  for (i = ptr_array->len - 1; i > index; i--)
    ptr_array->pdata[i] = g_ptr_array_index (ptr_array, i - 1);

  ptr_array->pdata[i] = data;
}

typedef gboolean (* GPredicate) (gpointer data);

static guint
g_list_length_predicate (GList      *list,
                         GPredicate  predicate)
{
  guint n;

  for (n = 0; list != NULL; list = g_list_next (list))
    if (predicate == NULL || predicate (list->data))
      n++;

  return n;
}

static guint
g_list_count_predicate (GList      *list,
                        guint       count,
                        GPredicate  predicate)
{
  guint n;

  if (predicate == NULL)
    return count;

  for (n = 0; count-- > 0 && list != NULL; list = g_list_next (list))
    if (predicate (list->data))
      n++;

  return n;
}

static GList *
g_list_next_predicate (GList      *list,
                       GPredicate  predicate)
{
  if (list != NULL)
    {
      do list = g_list_next (list);
      while (list != NULL && predicate != NULL && !predicate (list->data));
    }

  return list;
}

static GList *
g_list_first_predicate (GList      *list,
                        GPredicate  predicate)
{
  if (list != NULL && predicate != NULL && !predicate (list->data))
    list = g_list_next_predicate (list, predicate);

  return list;
}

static GList *
g_list_nth_predicate (GList      *list,
                      guint       n,
                      GPredicate  predicate)
{
  list = g_list_first_predicate (list, predicate);

  while (n--)
    list = g_list_next_predicate (list, predicate);

  return list;
}

static gboolean
gtk_menu_item_is_valid (gpointer data)
{
  return GTK_IS_MENU_ITEM (data) && !GTK_IS_TEAROFF_MENU_ITEM (data);
}

static void
unity_gtk_menu_item_handle_notify (GObject    *object,
                                   GParamSpec *pspec,
                                   gpointer    user_data)
{
  static const gchar *parent_name;
  static const gchar *submenu_name;

  UnityGtkMenuItem *item;
  GtkMenuItem *menu_item;
  UnityGtkMenuSection *parent_section;
  const gchar *pspec_name;

  g_return_if_fail (UNITY_GTK_IS_MENU_ITEM (user_data));

  item = user_data;
  menu_item = item->menu_item;
  parent_section = item->parent_section;
  pspec_name = g_param_spec_get_name (pspec);

  g_warn_if_fail (object == G_OBJECT (menu_item));

  if (parent_name == NULL)
    parent_name = g_intern_static_string ("parent");
  if (submenu_name == NULL)
    submenu_name = g_intern_static_string ("submenu");

  if (pspec_name == parent_name)
    {
      if (menu_item != NULL && gtk_widget_get_parent (GTK_WIDGET (menu_item)) == NULL)
        {
          if (parent_section != NULL && parent_section->parent_menu != NULL)
            unity_gtk_menu_remove_item (parent_section->parent_menu, item);
        }
    }
  else if (pspec_name == submenu_name)
    {
      gboolean submenu_changed;

      if (item->submenu_valid)
        {
          GtkMenuShell *old_menu_shell;
          GtkMenuShell *new_menu_shell;

          if (item->submenu != NULL)
            old_menu_shell = item->submenu->priv->menu_shell;
          else
            old_menu_shell = NULL;

          if (menu_item != NULL)
            new_menu_shell = GTK_MENU_SHELL (gtk_menu_item_get_submenu (menu_item));
          else
            new_menu_shell = NULL;

          submenu_changed = new_menu_shell != old_menu_shell;
        }
      else
        submenu_changed = FALSE;

      if (submenu_changed)
        {
          if (item->submenu_valid)
            {
              if (item->submenu != NULL)
                {
                  g_object_unref (item->submenu);
                  item->submenu = NULL;
                }

              item->submenu_valid = FALSE;
            }

          if (parent_section != NULL && parent_section->items != NULL)
            {
              gint i = g_ptr_array_find (parent_section->items, item);
              g_assert (i >= 0);
              g_menu_model_items_changed (G_MENU_MODEL (parent_section), i, 1, 1);
            }
        }
    }
}

static void
unity_gtk_menu_item_set_menu_item (UnityGtkMenuItem *item,
                                   GtkMenuItem      *menu_item)
{
  g_return_if_fail (UNITY_GTK_IS_MENU_ITEM (item));

  if (menu_item != item->menu_item)
    {
      if (item->menu_item_notify_handler_id)
        {
          g_assert (item->menu_item != NULL);
          g_signal_handler_disconnect (item->menu_item, item->menu_item_notify_handler_id);
          item->menu_item_notify_handler_id = 0;
        }

      item->menu_item = menu_item;

      if (menu_item != NULL)
        item->menu_item_notify_handler_id = g_signal_connect (menu_item, "notify", G_CALLBACK (unity_gtk_menu_item_handle_notify), item);
    }
}

static void
unity_gtk_menu_item_set_parent_section (UnityGtkMenuItem    *item,
                                        UnityGtkMenuSection *parent_section)
{
  g_return_if_fail (UNITY_GTK_IS_MENU_ITEM (item));

  if (parent_section != item->parent_section)
    {
      if (item->parent_section != NULL)
        {
          g_object_unref (item->parent_section);
          item->parent_section = NULL;
        }

      if (parent_section != NULL)
        item->parent_section = g_object_ref (parent_section);
    }
}

static UnityGtkMenu *
unity_gtk_menu_item_get_submenu (UnityGtkMenuItem *item)
{
  g_return_val_if_fail (UNITY_GTK_IS_MENU_ITEM (item), NULL);

  if (!item->submenu_valid)
    {
      if (item->submenu != NULL)
        {
          g_warn_if_reached ();
          g_object_unref (item->submenu);
          item->submenu = NULL;
        }

      if (item->menu_item != NULL)
        {
          GtkWidget *submenu = gtk_menu_item_get_submenu (item->menu_item);

          if (submenu != NULL)
            item->submenu = unity_gtk_menu_new (GTK_MENU_SHELL (submenu));
        }

      item->submenu_valid = TRUE;
    }

  return item->submenu;
}

static void
unity_gtk_menu_item_dispose (GObject *object)
{
  UnityGtkMenuItem *item;

  g_return_if_fail (UNITY_GTK_IS_MENU_ITEM (object));

  item = UNITY_GTK_MENU_ITEM (object);

  if (item->submenu != NULL)
    {
      g_warn_if_fail (item->submenu_valid);
      g_object_unref (item->submenu);
      item->submenu_valid = FALSE;
      item->submenu = NULL;
    }

  unity_gtk_menu_item_set_parent_section (item, NULL);
  unity_gtk_menu_item_set_menu_item (item, NULL);

  G_OBJECT_CLASS (unity_gtk_menu_item_parent_class)->dispose (object);
}

static void
unity_gtk_menu_item_get_property (GObject    *object,
                                  guint       property_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  UnityGtkMenuItem *self;

  g_return_if_fail (UNITY_GTK_IS_MENU_ITEM (object));

  self = UNITY_GTK_MENU_ITEM (object);

  switch (property_id)
    {
    case MENU_ITEM_PROP_MENU_ITEM:
      g_value_set_pointer (value, self->menu_item);
      break;

    case MENU_ITEM_PROP_PARENT_SECTION:
      g_value_set_object (value, self->parent_section);
      break;

    case MENU_ITEM_PROP_SUBMENU:
      g_value_set_object (value, unity_gtk_menu_item_get_submenu (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
unity_gtk_menu_item_set_property (GObject      *object,
                                  guint         property_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  UnityGtkMenuItem *self;

  g_return_if_fail (UNITY_GTK_IS_MENU_ITEM (object));

  self = UNITY_GTK_MENU_ITEM (object);

  switch (property_id)
    {
    case MENU_ITEM_PROP_MENU_ITEM:
      unity_gtk_menu_item_set_menu_item (self, g_value_get_pointer (value));
      break;

    case MENU_ITEM_PROP_PARENT_SECTION:
      unity_gtk_menu_item_set_parent_section (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
unity_gtk_menu_item_class_init (UnityGtkMenuItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = unity_gtk_menu_item_dispose;
  object_class->get_property = unity_gtk_menu_item_get_property;
  object_class->set_property = unity_gtk_menu_item_set_property;

  menu_item_properties[MENU_ITEM_PROP_MENU_ITEM] = g_param_spec_pointer ("menu-item",
                                                                         "Menu item",
                                                                         "Menu item",
                                                                         G_PARAM_READWRITE);
  menu_item_properties[MENU_ITEM_PROP_PARENT_SECTION] = g_param_spec_object ("parent-section",
                                                                             "Parent section",
                                                                             "Parent section",
                                                                             UNITY_GTK_TYPE_MENU_SECTION,
                                                                             G_PARAM_READWRITE);
  menu_item_properties[MENU_ITEM_PROP_SUBMENU] = g_param_spec_object ("submenu",
                                                                      "Submenu",
                                                                      "Submenu",
                                                                      UNITY_GTK_TYPE_MENU,
                                                                      G_PARAM_READABLE);

  g_object_class_install_properties (object_class, MENU_ITEM_N_PROPERTIES, menu_item_properties);
}

static void
unity_gtk_menu_item_init (UnityGtkMenuItem *self)
{
}

static UnityGtkMenuItem *
unity_gtk_menu_item_new (GtkMenuItem         *menu_item,
                         UnityGtkMenuSection *parent_section)
{
  return g_object_new (UNITY_GTK_TYPE_MENU_ITEM,
                       "menu-item", menu_item,
                       "parent-section", parent_section,
                       NULL);
}

static void
unity_gtk_menu_section_set_parent_menu (UnityGtkMenuSection *section,
                                        UnityGtkMenu        *parent_menu)
{
  g_return_if_fail (UNITY_GTK_IS_MENU_SECTION (section));

  if (parent_menu != section->parent_menu)
    {
      if (section->items != NULL)
        {
          g_ptr_array_unref (section->items);
          section->items = NULL;
        }

      if (section->parent_menu != NULL)
        {
          g_object_unref (section->parent_menu);
          section->parent_menu = NULL;
        }

      if (parent_menu != NULL)
        section->parent_menu = g_object_ref (parent_menu);
    }
}

static void
unity_gtk_menu_section_set_shell_offset (UnityGtkMenuSection *section,
                                         guint                shell_offset)
{
  g_return_if_fail (UNITY_GTK_IS_MENU_SECTION (section));

  if (shell_offset != section->shell_offset)
    {
      if (section->items != NULL)
        {
          g_ptr_array_unref (section->items);
          section->items = NULL;
        }

      section->shell_offset = shell_offset;
    }
}

static GPtrArray *
unity_gtk_menu_section_get_items (UnityGtkMenuSection *section)
{
  g_return_val_if_fail (UNITY_GTK_IS_MENU_SECTION (section), NULL);

  if (section->items == NULL && section->parent_menu != NULL && section->parent_menu->priv->menu_shell != NULL)
    {
      GtkMenuShell *menu_shell;
      GList *start;
      GList *finish;
      GList *iter;
      guint n;

      menu_shell = section->parent_menu->priv->menu_shell;
      start = gtk_container_get_children (GTK_CONTAINER (menu_shell));
      start = g_list_nth_predicate (start, section->shell_offset, gtk_menu_item_is_valid);
      finish = start;

      for (n = 0; finish != NULL && !GTK_IS_SEPARATOR_MENU_ITEM (finish->data); n++)
        finish = g_list_next_predicate (finish, gtk_menu_item_is_valid);

      /* Store the separator too. */
      if (finish != NULL)
        {
          finish = g_list_next_predicate (finish, gtk_menu_item_is_valid);
          n++;
        }

      section->items = g_ptr_array_new_full (n, g_object_unref);

      for (iter = start; iter != finish; iter = g_list_next_predicate (iter, gtk_menu_item_is_valid))
        g_ptr_array_add (section->items, unity_gtk_menu_item_new (iter->data, section));
    }

  return section->items;
}

static void
unity_gtk_menu_section_dispose (GObject *object)
{
  UnityGtkMenuSection *section;

  g_return_if_fail (UNITY_GTK_IS_MENU_SECTION (object));

  section = UNITY_GTK_MENU_SECTION (object);

  unity_gtk_menu_section_set_parent_menu (section, NULL);

  G_OBJECT_CLASS (unity_gtk_menu_section_parent_class)->dispose (object);
}

static void
unity_gtk_menu_section_get_property (GObject    *object,
                                     guint       property_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  UnityGtkMenuSection *self;

  g_return_if_fail (UNITY_GTK_IS_MENU_SECTION (object));

  self = UNITY_GTK_MENU_SECTION (object);

  switch (property_id)
    {
    case MENU_SECTION_PROP_PARENT_MENU:
      g_value_set_object (value, self->parent_menu);
      break;

    case MENU_SECTION_PROP_SHELL_OFFSET:
      g_value_set_uint (value, self->shell_offset);
      break;

    case MENU_SECTION_PROP_ITEMS:
      g_value_set_pointer (value, unity_gtk_menu_section_get_items (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
unity_gtk_menu_section_set_property (GObject      *object,
                                     guint         property_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  UnityGtkMenuSection *self;

  g_return_if_fail (UNITY_GTK_IS_MENU_SECTION (object));

  self = UNITY_GTK_MENU_SECTION (object);

  switch (property_id)
    {
    case MENU_SECTION_PROP_PARENT_MENU:
      unity_gtk_menu_section_set_parent_menu (self, g_value_get_object (value));
      break;

    case MENU_SECTION_PROP_SHELL_OFFSET:
      unity_gtk_menu_section_set_shell_offset (self, g_value_get_uint (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static gboolean
unity_gtk_menu_section_is_mutable (GMenuModel *model)
{
  g_return_val_if_fail (UNITY_GTK_IS_MENU_SECTION (model), TRUE);

  /* The menu can change at any time. */
  return TRUE;
}

static gint
unity_gtk_menu_section_get_n_items (GMenuModel *model)
{
  GPtrArray *items;

  g_return_val_if_fail (UNITY_GTK_IS_MENU_SECTION (model), 0);

  items = unity_gtk_menu_section_get_items (UNITY_GTK_MENU_SECTION (model));

  if (items->len > 0)
    {
      UnityGtkMenuItem *item = g_ptr_array_index (items, items->len - 1);

      if (item != NULL && GTK_IS_SEPARATOR_MENU_ITEM (item->menu_item))
        return items->len - 1;
      else
        return items->len;
    }

  return 0;
}

static void
unity_gtk_menu_section_get_item_attributes (GMenuModel  *model,
                                            gint         item_index,
                                            GHashTable **attributes)
{
  UnityGtkMenuSection *section;
  GPtrArray *items;
  GHashTable *hash_table;

  g_return_if_fail (UNITY_GTK_IS_MENU_SECTION (model));
  g_return_if_fail (0 <= item_index && item_index < g_menu_model_get_n_items (model));
  g_return_if_fail (attributes != NULL);

  section = UNITY_GTK_MENU_SECTION (model);
  items = unity_gtk_menu_section_get_items (section);
  hash_table = g_hash_table_new (g_str_hash, g_str_equal);

  if (items != NULL)
    {
      UnityGtkMenuItem *item = g_ptr_array_index (items, item_index);

      if (item != NULL)
        {
          GtkMenuItem *menu_item = item->menu_item;

          if (menu_item != NULL)
            {
              const gchar *label = gtk_menu_item_get_label (menu_item);

              if (label != NULL)
                g_hash_table_insert (hash_table, G_MENU_ATTRIBUTE_LABEL, g_variant_new_string (label));
            }
        }
      else
        /* We shouldn't have any NULLs. */
        g_assert_not_reached ();
    }

  *attributes = hash_table;
}

static void
unity_gtk_menu_section_get_item_links (GMenuModel  *model,
                                       gint         item_index,
                                       GHashTable **links)
{
  UnityGtkMenuSection *section;
  GPtrArray *items;
  GHashTable *hash_table;

  g_return_if_fail (UNITY_GTK_IS_MENU_SECTION (model));
  g_return_if_fail (0 <= item_index && item_index < g_menu_model_get_n_items (model));
  g_return_if_fail (links != NULL);

  section = UNITY_GTK_MENU_SECTION (model);
  items = unity_gtk_menu_section_get_items (section);
  hash_table = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);

  if (items != NULL)
    {
      UnityGtkMenuItem *item = g_ptr_array_index (items, item_index);

      if (item != NULL)
        {
          UnityGtkMenu *submenu = unity_gtk_menu_item_get_submenu (item);

          if (submenu != NULL)
            g_hash_table_insert (hash_table, G_MENU_LINK_SUBMENU, g_object_ref (submenu));
        }
      else
        /* We shouldn't have any NULLs. */
        g_assert_not_reached ();
    }

  *links = hash_table;
}

static void
unity_gtk_menu_section_class_init (UnityGtkMenuSectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GMenuModelClass *menu_model_class = G_MENU_MODEL_CLASS (klass);

  object_class->dispose = unity_gtk_menu_section_dispose;
  object_class->get_property = unity_gtk_menu_section_get_property;
  object_class->set_property = unity_gtk_menu_section_set_property;
  menu_model_class->is_mutable = unity_gtk_menu_section_is_mutable;
  menu_model_class->get_n_items = unity_gtk_menu_section_get_n_items;
  menu_model_class->get_item_attributes = unity_gtk_menu_section_get_item_attributes;
  menu_model_class->get_item_links = unity_gtk_menu_section_get_item_links;

  menu_section_properties[MENU_SECTION_PROP_PARENT_MENU] = g_param_spec_object ("parent-menu",
                                                                                "Parent menu",
                                                                                "Parent menu",
                                                                                UNITY_GTK_TYPE_MENU,
                                                                                G_PARAM_READWRITE);
  menu_section_properties[MENU_SECTION_PROP_SHELL_OFFSET] = g_param_spec_uint ("shell-offset",
                                                                               "Shell offset",
                                                                               "Shell offset",
                                                                               0,
                                                                               G_MAXUINT,
                                                                               0,
                                                                               G_PARAM_READWRITE);
  menu_section_properties[MENU_SECTION_PROP_ITEMS] = g_param_spec_pointer ("items",
                                                                           "Items",
                                                                           "Items",
                                                                           G_PARAM_READABLE);

  g_object_class_install_properties (object_class, MENU_SECTION_N_PROPERTIES, menu_section_properties);
}

static void
unity_gtk_menu_section_init (UnityGtkMenuSection *self)
{
}

static UnityGtkMenuSection *
unity_gtk_menu_section_new (UnityGtkMenu *parent_menu,
                            guint         shell_offset)
{
  return g_object_new (UNITY_GTK_TYPE_MENU_SECTION,
                       "parent-menu", parent_menu,
                       "shell-offset", shell_offset,
                       NULL);
}

static void
unity_gtk_menu_handle_insert (GtkMenuShell *menu_shell,
                              GtkWidget    *child,
                              gint          position,
                              gpointer      user_data)
{
  g_return_if_fail (UNITY_GTK_IS_MENU (user_data));

  if (gtk_menu_item_is_valid (child))
    {
      UnityGtkMenu *menu = user_data;
      UnityGtkMenuPrivate *priv = menu->priv;
      GPtrArray *sections = priv->sections;
      UnityGtkMenuSection *section;
      guint section_index;
      GList *menu_items;
      guint i;

      g_return_if_fail (sections != NULL);
      g_warn_if_fail (menu_shell == priv->menu_shell);
      g_warn_if_fail (position >= -1);

      menu_shell = priv->menu_shell;
      menu_items = gtk_container_get_children (GTK_CONTAINER (menu_shell));

      if (position < 0)
        position = g_list_length_predicate (menu_items, gtk_menu_item_is_valid);
      else
        position = g_list_count_predicate (menu_items, position, gtk_menu_item_is_valid);

      /* Find the index of the modified section. */
      for (section_index = 0; section_index + 1 < sections->len; section_index++)
        {
          section = g_ptr_array_index (sections, section_index + 1);
          if (section->shell_offset > position)
            break;
        }

      /* Shift shell offsets up by 1. */
      for (i = section_index + 1; i < sections->len; i++)
        {
          section = g_ptr_array_index (sections, i);
          section->shell_offset++;
        }

      section = g_ptr_array_index (sections, section_index);

      if (GTK_IS_SEPARATOR_MENU_ITEM (child))
        {
          UnityGtkMenuSection *new_section = unity_gtk_menu_section_new (menu, position + 1);

          g_ptr_array_insert (sections, new_section, section_index + 1);

          if (section->items != NULL)
            {
              guint size = position - section->shell_offset;
              guint new_size = section->items->len - size;
              UnityGtkMenuItem *item;
              gboolean has_separator;

              new_section->items = g_ptr_array_new_full (new_size, g_object_unref);

              /* Move menu items to the new section. */
              for (i = 0; i < new_size; i++)
                {
                  item = g_ptr_array_index (section->items, size + i);
                  unity_gtk_menu_item_set_parent_section (item, new_section);
                  g_ptr_array_add (new_section->items, g_object_ref (item));
                }

              g_ptr_array_set_size (section->items, size + 1);
              section->items->pdata[size] = unity_gtk_menu_item_new (GTK_MENU_ITEM (child), section);
              has_separator = item != NULL && GTK_IS_SEPARATOR_MENU_ITEM (item->menu_item);

              if (new_size > has_separator)
                g_menu_model_items_changed (G_MENU_MODEL (section), size, new_size - has_separator, 0);
            }

          g_menu_model_items_changed (G_MENU_MODEL (menu), section_index + 1, 0, 1);
        }
      else if (section->items != NULL)
        {
          UnityGtkMenuItem *item = unity_gtk_menu_item_new (GTK_MENU_ITEM (child), section);

          i = position - section->shell_offset;

          g_ptr_array_insert (section->items, item, i);

          g_menu_model_items_changed (G_MENU_MODEL (section), i, 0, 1);
        }

      if (!unity_gtk_menu_is_valid (menu))
        unity_gtk_menu_print (menu, 0);
    }
}

static void
unity_gtk_menu_remove_item (UnityGtkMenu     *menu,
                            UnityGtkMenuItem *item)
{
  GPtrArray *sections;
  UnityGtkMenuSection *section;
  GtkMenuItem *menu_item;

  g_return_if_fail (UNITY_GTK_IS_MENU (menu));
  g_return_if_fail (UNITY_GTK_IS_MENU_ITEM (item));

  sections = menu->priv->sections;
  section = item->parent_section;
  menu_item = item->menu_item;

  /* Try to find the parent section. */
  if (section == NULL && sections != NULL)
    {
      guint i;

      for (i = 0; i < sections->len; i++)
        {
          section = g_ptr_array_index (sections, i);

          if (section->items != NULL && g_ptr_array_find (section->items, item) >= 0)
            break;
        }

      if (i == sections->len)
        section = NULL;
    }

  g_return_if_fail (section != NULL);

  if (sections != NULL)
    {
      gint section_index = g_ptr_array_find (sections, section);
      UnityGtkMenuSection *next_section;
      guint i;

      /* The parent section should be in the menu. */
      g_assert (section_index >= 0);

      /* Shift shell offsets down by 1. */
      for (i = section_index + 1; i < sections->len; i++)
        {
          next_section = g_ptr_array_index (sections, i);
          next_section->shell_offset--;
        }

      /* Merge sections if removing a separator. */
      if (GTK_IS_SEPARATOR_MENU_ITEM (menu_item))
        {
          /* There should be a section after a separator. */
          g_assert (section_index + 1 < sections->len);

          next_section = g_ptr_array_index (sections, section_index + 1);

          /* If one section has items, the other should as well. */
          if (section->items != NULL || next_section->items != NULL)
            {
              unity_gtk_menu_section_get_items (section);
              unity_gtk_menu_section_get_items (next_section);
            }

          /* If one section has items, the other should as well. */
          g_assert ((section->items == NULL) == (next_section->items == NULL));

          if (section->items != NULL && next_section->items != NULL)
            {
              guint separator_index = section->items->len - 1;
              guint next_section_length = g_menu_model_get_n_items (G_MENU_MODEL (next_section));

              /* The separator should be at the end. */
              g_assert (item == g_ptr_array_index (section->items, separator_index));

              g_object_unref (item);
              section->items->pdata[separator_index] = NULL;
              g_ptr_array_set_size (section->items, separator_index + next_section->items->len);

              for (i = 0; i < next_section->items->len; i++)
                {
                  UnityGtkMenuItem *next_section_item = g_ptr_array_index (next_section->items, i);
                  unity_gtk_menu_item_set_parent_section (next_section_item, section);
                  section->items->pdata[separator_index + i] = g_object_ref (next_section_item);
                }

              if (next_section_length > 0)
                g_menu_model_items_changed (G_MENU_MODEL (section), separator_index, 0, next_section_length);
            }

          g_ptr_array_remove_index (sections, section_index + 1);

          g_menu_model_items_changed (G_MENU_MODEL (menu), section_index + 1, 1, 0);
        }
    }

  if (section->items != NULL && !GTK_IS_SEPARATOR_MENU_ITEM (menu_item))
    {
      gint item_index = g_ptr_array_find (section->items, item);

      /* The item should be in its section. */
      g_assert (item_index >= 0);

      g_ptr_array_remove_index (section->items, item_index);

      g_menu_model_items_changed (G_MENU_MODEL (section), item_index, 1, 0);
    }

  if (!unity_gtk_menu_is_valid (menu))
    unity_gtk_menu_print (menu, 0);
}

static void
unity_gtk_menu_set_menu_shell (UnityGtkMenu *menu,
                               GtkMenuShell *menu_shell)
{
  UnityGtkMenuPrivate *priv;

  g_return_if_fail (UNITY_GTK_IS_MENU (menu));

  priv = menu->priv;

  if (menu_shell != priv->menu_shell)
    {
      if (priv->sections != NULL)
        {
          if (priv->menu_shell_insert_handler_id)
            {
              g_assert (priv->menu_shell != NULL);
              g_signal_handler_disconnect (priv->menu_shell, priv->menu_shell_insert_handler_id);
              priv->menu_shell_insert_handler_id = 0;
            }

          g_ptr_array_unref (priv->sections);
          priv->sections = NULL;
        }

      priv->menu_shell = menu_shell;
    }
}

static GPtrArray *
unity_gtk_menu_get_sections (UnityGtkMenu *menu)
{
  UnityGtkMenuPrivate *priv;

  g_return_val_if_fail (UNITY_GTK_IS_MENU (menu), NULL);

  priv = menu->priv;

  if (priv->sections == NULL)
    {
      GtkMenuShell *menu_shell = priv->menu_shell;

      priv->sections = g_ptr_array_new_with_free_func (g_object_unref);

      if (menu_shell != NULL)
        {
          GList *iter;
          guint i;

          iter = gtk_container_get_children (GTK_CONTAINER (menu_shell));
          iter = g_list_first_predicate (iter, gtk_menu_item_is_valid);

          if (iter != NULL)
            {
              for (i = 0; iter != NULL; i++)
                {
                  g_ptr_array_add (priv->sections, unity_gtk_menu_section_new (menu, i));

                  for (; iter != NULL && !GTK_IS_SEPARATOR_MENU_ITEM (iter->data); i++)
                    iter = g_list_next_predicate (iter, gtk_menu_item_is_valid);

                  if (iter != NULL)
                    {
                      iter = g_list_next_predicate (iter, gtk_menu_item_is_valid);

                      /* We have a trailing separator. */
                      if (iter == NULL)
                        g_ptr_array_add (priv->sections, unity_gtk_menu_section_new (menu, i + 1));
                    }
                }
            }
          else
            g_ptr_array_add (priv->sections, unity_gtk_menu_section_new (menu, 0));

          priv->menu_shell_insert_handler_id = g_signal_connect (menu_shell, "insert", G_CALLBACK (unity_gtk_menu_handle_insert), menu);
        }
    }

  return priv->sections;
}

static void
unity_gtk_menu_dispose (GObject *object)
{
  UnityGtkMenu *menu;

  g_return_if_fail (UNITY_GTK_IS_MENU (object));

  menu = UNITY_GTK_MENU (object);

  unity_gtk_menu_set_menu_shell (menu, NULL);

  G_OBJECT_CLASS (unity_gtk_menu_parent_class)->dispose (object);
}

static void
unity_gtk_menu_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  UnityGtkMenu *self;
  UnityGtkMenuPrivate *priv;

  g_return_if_fail (UNITY_GTK_IS_MENU (object));

  self = UNITY_GTK_MENU (object);
  priv = self->priv;

  switch (property_id)
    {
    case MENU_PROP_MENU_SHELL:
      g_value_set_pointer (value, priv->menu_shell);
      break;

    case MENU_PROP_SECTIONS:
      g_value_set_pointer (value, unity_gtk_menu_get_sections (self));
      break;

    case MENU_PROP_ACTION_GROUP:
      g_value_set_object (value, priv->action_group);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
unity_gtk_menu_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  UnityGtkMenu *self;

  g_return_if_fail (UNITY_GTK_IS_MENU (object));

  self = UNITY_GTK_MENU (object);

  switch (property_id)
    {
    case MENU_PROP_MENU_SHELL:
      unity_gtk_menu_set_menu_shell (self, g_value_get_pointer (value));
      break;

    case MENU_PROP_ACTION_GROUP:
      unity_gtk_action_group_add_menu (UNITY_GTK_ACTION_GROUP (g_value_get_object (value)), self);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static gboolean
unity_gtk_menu_is_mutable (GMenuModel *model)
{
  g_return_val_if_fail (UNITY_GTK_IS_MENU (model), TRUE);

  /* The menu can change at any time. */
  return TRUE;
}

static gint
unity_gtk_menu_get_n_items (GMenuModel *model)
{
  g_return_val_if_fail (UNITY_GTK_IS_MENU (model), 0);

  return unity_gtk_menu_get_sections (UNITY_GTK_MENU (model))->len;
}

static void
unity_gtk_menu_get_item_attributes (GMenuModel  *model,
                                    gint         item_index,
                                    GHashTable **attributes)
{
  g_return_if_fail (UNITY_GTK_IS_MENU (model));
  g_return_if_fail (0 <= item_index && item_index < g_menu_model_get_n_items (model));
  g_return_if_fail (attributes != NULL);

  *attributes = g_hash_table_new (g_str_hash, g_str_equal);
}

static void
unity_gtk_menu_get_item_links (GMenuModel  *model,
                               gint         item_index,
                               GHashTable **links)
{
  UnityGtkMenu *menu;
  GPtrArray *sections;
  UnityGtkMenuSection *section;
  GHashTable *hash_table;

  g_return_if_fail (UNITY_GTK_IS_MENU (model));
  g_return_if_fail (0 <= item_index && item_index < g_menu_model_get_n_items (model));
  g_return_if_fail (links != NULL);

  menu = UNITY_GTK_MENU (model);
  sections = unity_gtk_menu_get_sections (menu);
  section = g_ptr_array_index (sections, item_index);
  hash_table = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);

  if (section != NULL)
    g_hash_table_insert (hash_table, G_MENU_LINK_SECTION, g_object_ref (section));

  *links = hash_table;
}

static void
unity_gtk_menu_class_init (UnityGtkMenuClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GMenuModelClass *menu_model_class = G_MENU_MODEL_CLASS (klass);

  object_class->dispose = unity_gtk_menu_dispose;
  object_class->get_property = unity_gtk_menu_get_property;
  object_class->set_property = unity_gtk_menu_set_property;
  menu_model_class->is_mutable = unity_gtk_menu_is_mutable;
  menu_model_class->get_n_items = unity_gtk_menu_get_n_items;
  menu_model_class->get_item_attributes = unity_gtk_menu_get_item_attributes;
  menu_model_class->get_item_links = unity_gtk_menu_get_item_links;

  menu_properties[MENU_PROP_MENU_SHELL] = g_param_spec_pointer ("menu-shell",
                                                                "Menu shell",
                                                                "Menu shell",
                                                                G_PARAM_READWRITE);
  menu_properties[MENU_PROP_SECTIONS] = g_param_spec_pointer ("sections",
                                                              "Sections",
                                                              "Sections",
                                                              G_PARAM_READABLE);
  menu_properties[MENU_PROP_ACTION_GROUP] = g_param_spec_object ("action-group",
                                                                 "Action group",
                                                                 "Action group",
                                                                 UNITY_GTK_TYPE_ACTION_GROUP,
                                                                 G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, MENU_N_PROPERTIES, menu_properties);

  g_type_class_add_private (klass, sizeof (UnityGtkMenuPrivate));
}

static void
unity_gtk_menu_init (UnityGtkMenu *self)
{
  self->priv = UNITY_GTK_MENU_GET_PRIVATE (self);
}

UnityGtkMenu *
unity_gtk_menu_new (GtkMenuShell *menu_shell)
{
  return g_object_new (UNITY_GTK_TYPE_MENU,
                       "menu-shell", menu_shell,
                       NULL);
}

static gboolean
unity_gtk_menu_item_is_valid (UnityGtkMenuItem *item)
{
  guint i;

  g_return_val_if_fail (item != NULL, FALSE);
  g_return_val_if_fail (UNITY_GTK_IS_MENU_SECTION (item->parent_section), FALSE);
  g_return_val_if_fail (item->menu_item == NULL || GTK_IS_MENU_ITEM (item->menu_item), FALSE);
  g_return_val_if_fail (!!item->menu_item_notify_handler_id == GTK_IS_MENU_ITEM (item->menu_item), FALSE);
  g_return_val_if_fail (!item->submenu_valid || item->submenu == NULL || UNITY_GTK_IS_MENU (item->submenu), FALSE);
  g_return_val_if_fail (item->parent_section->items != NULL, FALSE);

  /* A menu item must be contained in its parent section. */
  for (i = 0; i < item->parent_section->items->len; i++)
    if (g_ptr_array_index (item->parent_section->items, i) == item)
      break;

  g_return_val_if_fail (i < item->parent_section->items->len, FALSE);

  /* Separators must appear at the end of their parent sections. */
  g_return_val_if_fail (!GTK_IS_SEPARATOR_MENU_ITEM (item->menu_item) || g_ptr_array_index (item->parent_section->items, item->parent_section->items->len - 1) == item, FALSE);

  /* The menu item should match what's in the shell. */
  if (item->menu_item != NULL)
    {
      GList *iter;

      g_return_val_if_fail (UNITY_GTK_IS_MENU (item->parent_section->parent_menu), FALSE);
      g_return_val_if_fail (GTK_IS_MENU_SHELL (item->parent_section->parent_menu->priv->menu_shell), FALSE);

      iter = g_list_nth (gtk_container_get_children (GTK_CONTAINER (item->parent_section->parent_menu->priv->menu_shell)), item->parent_section->shell_offset);

      if (item->parent_section->items->len > 0)
        {
          for (i = 0; i < item->parent_section->items->len; i++)
            {
              if (g_ptr_array_index (item->parent_section->items, i) == item)
                {
                  g_return_val_if_fail (iter != NULL, FALSE);
                  g_return_val_if_fail (item->menu_item == iter->data, FALSE);

                  break;
                }

              iter = g_list_next (iter);
            }

          g_return_val_if_fail (i < item->parent_section->items->len, FALSE);
        }
    }

  if (item->submenu_valid && UNITY_GTK_IS_MENU (item->submenu))
    return unity_gtk_menu_is_valid (item->submenu);

  return TRUE;
}

static gboolean
unity_gtk_menu_section_is_valid (UnityGtkMenuSection *section)
{
  guint i;

  g_return_val_if_fail (UNITY_GTK_IS_MENU_SECTION (section), FALSE);
  g_return_val_if_fail (UNITY_GTK_IS_MENU (section->parent_menu), FALSE);

  /* A section must be contained in its parent menu. */
  for (i = 0; i < section->parent_menu->priv->sections->len; i++)
    if (g_ptr_array_index (section->parent_menu->priv->sections, i) == section)
      break;

  g_return_val_if_fail (i < section->parent_menu->priv->sections->len, FALSE);

  if (section->items != NULL)
    {
      for (i = 0; i < section->items->len; i++)
        {
          UnityGtkMenuItem *item = g_ptr_array_index (section->items, i);

          g_return_val_if_fail (item != NULL, FALSE);

          /* This section is the parent of all of its items. */
          g_return_val_if_fail (item->parent_section == section, FALSE);

          /* Separators can only exist at the end of a section. */
          if (i < section->items->len - 1)
            g_return_val_if_fail (!GTK_IS_SEPARATOR_MENU_ITEM (item->menu_item), FALSE);
        }

      for (i = 0; i < section->items->len; i++)
        if (!unity_gtk_menu_item_is_valid (g_ptr_array_index (section->items, i)))
          return FALSE;
    }

  return TRUE;
}

gboolean
unity_gtk_menu_is_valid (UnityGtkMenu *menu)
{
  UnityGtkMenuPrivate *priv;

  g_return_val_if_fail (UNITY_GTK_IS_MENU (menu), FALSE);

  priv = menu->priv;

  g_return_val_if_fail (priv->menu_shell == NULL || GTK_IS_MENU_SHELL (priv->menu_shell), FALSE);
  g_return_val_if_fail (priv->sections == NULL || GTK_IS_MENU_SHELL (priv->menu_shell), FALSE);
  g_return_val_if_fail (!priv->menu_shell_insert_handler_id || priv->sections != NULL, FALSE);

  if (priv->sections != NULL)
    {
      guint i;

      for (i = 0; i < priv->sections->len; i++)
        {
          UnityGtkMenuSection *section = g_ptr_array_index (priv->sections, i);

          g_return_val_if_fail (UNITY_GTK_IS_MENU_SECTION (section), FALSE);

          /* This menu is the parent of all of its sections. */
          g_return_val_if_fail (section->parent_menu == menu, FALSE);

          if (section->items != NULL)
            {
              if (i + 1 < priv->sections->len)
                {
                  UnityGtkMenuSection *next_section = g_ptr_array_index (priv->sections, i + 1);
                  UnityGtkMenuItem *item;
                  GtkMenuItem *menu_item;

                  /* The shell offsets should be correct. */
                  g_return_val_if_fail (section->shell_offset + section->items->len == next_section->shell_offset, FALSE);

                  /* We should have at least one item. */
                  g_return_val_if_fail (section->items->len > 0, FALSE);

                  item = g_ptr_array_index (section->items, section->items->len - 1);
                  g_return_val_if_fail (item != NULL, FALSE);
                  menu_item = item->menu_item;

                  /* The last item should be a separator. */
                  g_return_val_if_fail (menu_item == NULL || GTK_IS_SEPARATOR_MENU_ITEM (menu_item), FALSE);
                }
              else if (section->items->len > 0)
                {
                  UnityGtkMenuItem *item = g_ptr_array_index (section->items, section->items->len - 1);

                  /* The last item should not be a separator. */
                  g_return_val_if_fail (!GTK_IS_SEPARATOR_MENU_ITEM (item->menu_item), FALSE);
                }
            }
        }

      for (i = 0; i < priv->sections->len; i++)
        if (!unity_gtk_menu_section_is_valid (g_ptr_array_index (priv->sections, i)))
          return FALSE;
    }

  return TRUE;
}

static void
unity_gtk_menu_item_print (UnityGtkMenuItem *item,
                           guint             depth)
{
  gchar *indent;
  GtkMenuItem *menu_item;
  const gchar *label;
  UnityGtkMenu *submenu;

  g_return_if_fail (item != NULL);

  indent = g_strnfill (2 + 3 * depth, ' ');
  menu_item = item->menu_item;
  label = menu_item != NULL ? gtk_menu_item_get_label (menu_item) : NULL;
  submenu = item->submenu;

  if (label != NULL && label[0] != '\0')
    g_print ("%s(%s *) %p \"%s\"\n", indent, G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (menu_item)), item, label);
  else
    g_print ("%s(%s *) %p\n", indent, G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (menu_item)), item);

  if (submenu != NULL)
    unity_gtk_menu_print (submenu, depth + 1);

  g_free (indent);
}

static void
unity_gtk_menu_section_print (UnityGtkMenuSection *section,
                              guint                depth)
{
  gchar *indent;
  GPtrArray *items;
  guint i;

  g_return_if_fail (UNITY_GTK_IS_MENU_SECTION (section));

  indent = g_strnfill (1 + 3 * depth, ' ');
  items = unity_gtk_menu_section_get_items (section);

  g_print ("%s(%s *) %p\n", indent, G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (section)), section);

  for (i = 0; i < items->len; i++)
    unity_gtk_menu_item_print (g_ptr_array_index (items, i), depth);

  g_free (indent);
}

void
unity_gtk_menu_print (UnityGtkMenu *menu,
                      guint         depth)
{
  gchar *indent;
  GPtrArray *sections;
  guint i;

  g_return_if_fail (UNITY_GTK_IS_MENU (menu));

  indent = g_strnfill (3 * depth, ' ');
  sections = unity_gtk_menu_get_sections (menu);

  g_print ("%s(%s *) %p\n", indent, G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (menu)), menu);

  for (i = 0; i < sections->len; i++)
    unity_gtk_menu_section_print (g_ptr_array_index (sections, i), depth);

  g_free (indent);
}

void
g_menu_model_print (GMenuModel *model,
                    guint       depth)
{
  gchar *indent;
  guint i;

  g_return_if_fail (G_IS_MENU_MODEL (model));

  indent = g_strnfill (3 * depth, ' ');

  g_print ("%s(%s *) %p\n", indent, G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (model)), model);

  for (i = 0; i < g_menu_model_get_n_items (model); i++)
    {
      GVariant *variant = g_menu_model_get_item_attribute_value (model, i, G_MENU_ATTRIBUTE_LABEL, G_VARIANT_TYPE_STRING);
      GMenuModel *section = g_menu_model_get_item_link (model, i, G_MENU_LINK_SECTION);
      GMenuModel *submenu = g_menu_model_get_item_link (model, i, G_MENU_LINK_SUBMENU);
      const gchar *label = NULL;

      if (variant != NULL)
        label = g_variant_get_string (variant, NULL);

      if (label != NULL)
        g_print ("%s %d: %s\n", indent, i, label);
      else
        g_print ("%s %d\n", indent, i);

      if (section != NULL)
        {
          g_print ("%s  section\n", indent);
          g_menu_model_print (section, depth + 1);
        }

      if (submenu != NULL)
        {
          g_print ("%s  submenu\n", indent);
          g_menu_model_print (submenu, depth + 1);
        }
    }

  g_free (indent);
}

static void
unity_gtk_action_group_class_init (UnityGtkActionGroupClass *klass)
{
  g_type_class_add_private (klass, sizeof (UnityGtkActionGroupPrivate));
}

static void
unity_gtk_action_group_g_action_group_init (GActionGroupInterface *iface)
{
}

static void
unity_gtk_action_group_init (UnityGtkActionGroup *self)
{
  self->priv = UNITY_GTK_ACTION_GROUP_GET_PRIVATE (self);
}

UnityGtkActionGroup *
unity_gtk_action_group_new (void)
{
  return g_object_new (UNITY_GTK_TYPE_ACTION_GROUP, NULL);
}

void
unity_gtk_action_group_add_menu (UnityGtkActionGroup *group,
                                 UnityGtkMenu        *menu)
{
  g_return_if_fail (UNITY_GTK_IS_ACTION_GROUP (group));
  g_return_if_fail (UNITY_GTK_IS_MENU (menu));

  if (menu->priv->action_group != group && menu->priv->action_group != NULL)
    unity_gtk_action_group_remove_menu (menu->priv->action_group, menu);

  if (menu->priv->sections != NULL)
    {
      guint i;

      for (i = 0; i < menu->priv->sections->len; i++)
        {
          UnityGtkMenuSection *section = g_ptr_array_index (menu->priv->sections, i);

          if (section->items != NULL)
            {
              guint j;

              for (j = 0; j < section->items->len; j++)
                {
                  UnityGtkMenuItem *item = g_ptr_array_index (section->items, j);

                  if (menu->priv->action_group != group)
                    {
                      /* XXX: Add the item's action. */
                    }

                  if (item->submenu_valid && item->submenu != NULL)
                    unity_gtk_action_group_add_menu (group, item->submenu);
                }
            }
        }
    }

  if (menu->priv->action_group != group)
    menu->priv->action_group = g_object_ref (group);
}

void
unity_gtk_action_group_remove_menu (UnityGtkActionGroup *group,
                                    UnityGtkMenu        *menu)
{
  g_return_if_fail (UNITY_GTK_IS_ACTION_GROUP (group));
  g_return_if_fail (UNITY_GTK_IS_MENU (menu));
  g_return_if_fail (menu->priv->action_group == group);

  g_object_unref (menu->priv->action_group);
  menu->priv->action_group = NULL;

  if (menu->priv->sections != NULL)
    {
      guint i;

      for (i = 0; i < menu->priv->sections->len; i++)
        {
          UnityGtkMenuSection *section = g_ptr_array_index (menu->priv->sections, i);

          if (section->items != NULL)
            {
              guint j;

              for (j = 0; j < section->items->len; j++)
                {
                  UnityGtkMenuItem *item = g_ptr_array_index (section->items, j);

                  /* XXX: Remove the item's action. */

                  if (item->submenu_valid && item->submenu != NULL)
                    unity_gtk_action_group_remove_menu (group, item->submenu);
                }
            }
        }
    }
}
