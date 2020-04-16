#include "base.hpp"

namespace {

PKG;

TEST(StructureTest, a_alloc_empty)
{
    auto* meta_struct = zsr::find<zsr::Compound>(pkg, "a_struct");

    ASSERT_TRUE(meta_struct);
    ASSERT_TRUE(meta_struct->alloc);
    ASSERT_FALSE(meta_struct->initialize);
    ASSERT_EQ(meta_struct->type, zsr::Compound::Type::Structure);

    auto introspectable = meta_struct->alloc();
    ASSERT_EQ(introspectable.meta(), meta_struct);
}

TEST(StructureTest, b_alloc_init_parameterized)
{
    auto* meta_struct = zsr::find<zsr::Compound>(pkg, "b_struct");

    ASSERT_TRUE(meta_struct);
    ASSERT_TRUE(meta_struct->alloc);
    ASSERT_TRUE(meta_struct->initialize);

    auto introspectable = meta_struct->alloc();

    /*  Initialize with parameter list */
    auto* meta_parameter_a = zsr::find<zsr::Parameter>(meta_struct, "a");

    ASSERT_TRUE(meta_parameter_a);

    zsr::ParameterList l;
    meta_parameter_a->set(l, uint8_t{123});

    meta_struct->initialize(introspectable, l);

    /* Read out matching field */
    auto* meta_field =
        zsr::find<zsr::Field>(meta_struct, meta_parameter_a->ident);

    ASSERT_TRUE(meta_field);
    ASSERT_TRUE(meta_field->get);

    ASSERT_VARIANT_EQ(meta_field->get(introspectable), uint8_t{123});
}

TEST(StructureTest, c_alloc_init_parameterized_instance)
{
    auto* s_parent = zsr::find<zsr::Compound>(pkg, "c_struct");
    auto* s_parameter = zsr::find<zsr::Compound>(pkg, "c_parameter_struct");

    ASSERT_TRUE(s_parent);
    ASSERT_TRUE(s_parameter);

    auto* p_a = zsr::find<zsr::Parameter>(s_parent, "a");

    ASSERT_TRUE(p_a);

    /* Set parameter instance */
    zsr::ParameterList l;
    auto i_parameter = s_parameter->alloc();

    auto* m_a = zsr::find<zsr::Field>(s_parameter, "a");

    m_a->set(i_parameter, "Hola");
    p_a->set(l, i_parameter);

    auto i_parent = s_parent->alloc();
    s_parent->initialize(i_parent, std::move(l));

    /* Read out (read-only) field */

    auto* m_parameter = zsr::find<zsr::Field>(s_parent, "a");

    ASSERT_TRUE(m_parameter);
    ASSERT_TRUE(m_parameter->get);

    auto value = m_parameter->get(i_parent);
    auto instance = value.get<zsr::Introspectable>();

    ASSERT_TRUE(instance);
    ASSERT_TRUE(instance->meta());

    auto* m_ca_a = zsr::find<zsr::Field>(s_parameter, "a");
    ASSERT_VARIANT_EQ(m_ca_a->get(*instance), std::string{"Hola"});
}

TEST(StructureTest, d_call_function)
{
    auto* s_struct = zsr::find<zsr::Compound>(pkg, "d_struct");
    auto* f_fun = zsr::find<zsr::Function>(s_struct, "fun");

    /* Alloc instance */
    auto instance = s_struct->alloc();

    /* Call function */
    auto value = f_fun->call(instance);
    ASSERT_VARIANT_EQ(value, uint8_t{10});
}

TEST(StructureTest, e_set_get_builtin_field)
{
    auto* s_struct = zsr::find<zsr::Compound>(pkg, "e_struct");
    auto* m_value = zsr::find<zsr::Field>(s_struct, "a");

    /* Alloc instance */
    auto instance = s_struct->alloc();

    /* Write & read parameter */
    m_value->set(instance, std::string{"Hello"});
    ASSERT_VARIANT_EQ(m_value->get(instance), std::string{"Hello"});

    /* Overwrite & read parameter */
    m_value->set(instance, std::string{"World"});
    ASSERT_VARIANT_EQ(m_value->get(instance), std::string{"World"});
}

TEST(StructureTest, f_set_get_compound_field)
{
    auto* s_parent = zsr::find<zsr::Compound>(pkg, "f_parent");
    auto* m_parent = zsr::find<zsr::Field>(s_parent, "a");
    auto* f_parent = zsr::find<zsr::Function>(s_parent, "child_value");

    auto* s_child = zsr::find<zsr::Compound>(pkg, "f_child");
    auto* m_child = zsr::find<zsr::Field>(s_child, "a");

    /* Alloc parameter */
    auto child = s_child->alloc();
    m_child->set(child, int32_t{123});

    /* Alloc parent & set child */
    auto parent = s_parent->alloc();
    m_parent->set(parent, child);

    /* Access child */
    auto child_ref = m_parent->get(parent);
    ASSERT_TRUE(child_ref.get<zsr::Introspectable>());

    /* Call function */
    auto value = f_parent->call(parent);
    ASSERT_VARIANT_EQ(value, int32_t{123});
}

TEST(StructureTest, g_set_get_builtin_array_field)
{
    auto* s_struct = zsr::find<zsr::Compound>(pkg, "g_struct");
    auto* m_value = zsr::find<zsr::Field>(s_struct, "a");

    /* Alloc instance */
    auto instance = s_struct->alloc();

    /* Write & read parameter */
    m_value->set(instance, std::vector<std::string>{"Hello", "World", "!"});

    if (auto result = m_value->get(instance).get<std::vector<std::string>>()) {
        auto value = *result;
        ASSERT_EQ(value[0], "Hello");
        ASSERT_EQ(value[1], "World");
        ASSERT_EQ(value[2], "!");
    } else {
        FAIL() << "Not a vector of string!";
    }
}

TEST(StructureTest, h_invalid_field_access)
{
    auto* meta_struct_a = zsr::find<zsr::Compound>(pkg, "h_struct_a");
    auto* meta_struct_b = zsr::find<zsr::Compound>(pkg, "h_struct_b");
    auto* meta_value = zsr::find<zsr::Field>(meta_struct_a, "a");

    /* Alloc instance b */
    auto instance = meta_struct_b->alloc();

    /* Try write a::a with (wrong) instance of b */
    EXPECT_THROW(meta_value->set(instance, std::string{"Hello"}),
                 zsr::IntrospectableCastError);
}

TEST(StructureTest, i_check_has_initialize_children)
{
    auto* meta_struct_a = zsr::find<zsr::Compound>(pkg, "i_struct_a");
    ASSERT_FALSE(meta_struct_a->initializeChildren);

    auto* meta_struct_b = zsr::find<zsr::Compound>(pkg, "i_struct_b");
    ASSERT_TRUE(meta_struct_b->initializeChildren);
}

TEST(StructureTest, j_conditional_field)
{
    auto* meta_struct_a = zsr::find<zsr::Compound>(pkg, "j_struct");
    auto* meta_field_a = zsr::find<zsr::Field>(meta_struct_a, "a");

    ASSERT_FALSE(meta_field_a->has);
    ASSERT_FALSE(meta_field_a->reset);

    auto* meta_field_b = zsr::find<zsr::Field>(meta_struct_a, "b");

    ASSERT_TRUE(meta_field_b->has);
    ASSERT_TRUE(meta_field_b->reset);
}

TEST(StructureTest, k_nested_compounds)
{
    auto* meta_struct_a = zsr::find<zsr::Compound>(pkg, "k_struct_a");
    auto* meta_field_a_a = zsr::find<zsr::Field>(meta_struct_a, "a");

    auto* meta_struct_b = zsr::find<zsr::Compound>(pkg, "k_struct_b");
    auto* meta_field_b_a = zsr::find<zsr::Field>(meta_struct_b, "a");

    /* Alloc root object b */
    auto instance_b = meta_struct_b->alloc();

    /* Add children of type a */
    std::vector<zsr::Introspectable> children;
    for (auto i = 0; i < 10; ++i) {
        auto child = meta_struct_a->alloc();
        meta_field_a_a->set(child, int32_t{i});

        children.push_back(child);
    }

    meta_field_b_a->set(instance_b, children);

    /* Get children, expecting non-owning objects */
    auto opt_children_a =
        meta_field_b_a->get(instance_b).get<std::vector<zsr::Introspectable>>();

    ASSERT_TRUE(opt_children_a);
    ASSERT_EQ(opt_children_a->size(), 10);

    {
        int32_t idx{0};
        for (const auto& child : *opt_children_a) {
            ASSERT_FALSE(child.isOwning());
            ASSERT_VARIANT_EQ(meta_field_a_a->get(child), idx);

            ++idx;
        }
    }

    /* Overwrite/clear the vector of instance b */
    meta_field_b_a->set(instance_b, std::vector<zsr::Introspectable>{});

    /* Expect all (prev) children to own their referenced object */
    {
        int32_t idx{0};
        for (const auto& child : *opt_children_a) {
            ASSERT_TRUE(child.isOwning());
            ASSERT_VARIANT_EQ(meta_field_a_a->get(child), idx);

            ++idx;
        }
    }
}

} // namespace