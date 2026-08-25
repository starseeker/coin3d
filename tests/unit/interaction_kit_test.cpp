#include <gtest/gtest.h>

#include <Inventor/nodekits/SoInteractionKit.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSeparator.h>

namespace {

class InteractionKitProbe : public SoInteractionKit {
public:
    using SoInteractionKit::setAnyPartAsDefault;
    using SoBaseKit::getAnyPart;

    void setPartDefault(const SbName & name, SbBool value)
    {
        const int part_number = this->getNodekitCatalog()->getPartNumber(name);
        ASSERT_GE(part_number, 0);
        this->getCatalogInstances()[part_number]->setDefault(value);
    }

    ~InteractionKitProbe() override = default;
};

} // namespace

TEST(InteractionKit, DefaultPartMutationHonorsVisibilityAndDefaultPolicy)
{
    auto * kit = new InteractionKitProbe;
    kit->ref();

    auto * separator = new SoSeparator;
    separator->ref();
    SoNode * original = kit->getAnyPart("topSeparator", TRUE, FALSE, FALSE);
    ASSERT_NE(original, nullptr);
    ASSERT_GE(kit->getChildren()->find(original), 0);
    ASSERT_EQ(kit->getChildren()->find(separator), -1);
    const int top_separator_number =
        kit->getNodekitCatalog()->getPartNumber("topSeparator");
    ASSERT_GE(top_separator_number, 0);
    EXPECT_EQ(separator->getTypeId(),
              kit->getNodekitCatalog()->getType(top_separator_number));
    EXPECT_TRUE(kit->setAnyPartAsDefault("topSeparator", separator, TRUE, FALSE));
    separator->unref();
    EXPECT_EQ(kit->getAnyPart("topSeparator", TRUE, FALSE, FALSE), separator);

    kit->setPartDefault("topSeparator", FALSE);
    auto * replacement = new SoSeparator;
    replacement->ref();
    EXPECT_FALSE(kit->setAnyPartAsDefault("topSeparator", replacement, TRUE, TRUE));
    EXPECT_EQ(kit->getAnyPart("topSeparator", TRUE, FALSE, FALSE), separator);
    EXPECT_TRUE(kit->setAnyPartAsDefault("topSeparator", replacement, TRUE, FALSE));
    replacement->unref();
    EXPECT_EQ(kit->getAnyPart("topSeparator", TRUE, FALSE, FALSE), replacement);

    auto * cube = new SoCube;
    cube->ref();
    EXPECT_FALSE(kit->setAnyPartAsDefault("topSeparator", cube, FALSE, FALSE));
    EXPECT_FALSE(kit->setAnyPartAsDefault("doesNotExist", cube, TRUE, FALSE));
    cube->unref();

    kit->unref();
}
