#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class LteTestSuite : public TestCase
{
public:
    LteTestSuite() : TestCase("Test LTE Stack with Simplex UDP Applications") {}
    virtual ~LteTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestMobilityModel();
        TestLteStackInstallation();
        TestIpv4AddressAssignment();
        TestUdpServerSetup();
        TestUdpClientSetup();
        TestDataTransmission();
    }

private:
    // Test node creation (verify that eNB and UE nodes are created)
    void TestNodeCreation()
    {
        NodeContainer eNBNode, ueNode;
        eNBNode.Create(1);
        ueNode.Create(1);

        // Verify that 1 eNB and 1 UE node are created
        NS_TEST_ASSERT_MSG_EQ(eNBNode.GetN(), 1, "Failed to create eNB node.");
        NS_TEST_ASSERT_MSG_EQ(ueNode.GetN(), 1, "Failed to create UE node.");
    }

    // Test mobility model (verify that the mobility models are installed)
    void TestMobilityModel()
    {
        NodeContainer eNBNode, ueNode;
        eNBNode.Create(1);
        ueNode.Create(1);

        MobilityHelper mobility;

        // eNB is static with constant position model
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(50.0),
                                      "DeltaY", DoubleValue(50.0),
                                      "GridWidth", UintegerValue(1),
                                      "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(eNBNode);

        // UE has random walk mobility model
        mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                      "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                      "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                  "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
        mobility.Install(ueNode);

        // Verify that mobility models are installed on both eNB and UE nodes
        Ptr<MobilityModel> eNBMobility = eNBNode.Get(0)->GetObject<MobilityModel>();
        Ptr<MobilityModel> ueMobility = ueNode.Get(0)->GetObject<MobilityModel>();

        NS_TEST_ASSERT_MSG_NE(eNBMobility, nullptr, "Failed to install mobility model on eNB node.");
        NS_TEST_ASSERT_MSG_NE(ueMobility, nullptr, "Failed to install mobility model on UE node.");
    }

    // Test LTE stack installation (verify that the LTE stack is installed on both eNB and UE nodes)
    void TestLteStackInstallation()
    {
        NodeContainer eNBNode, ueNode;
        eNBNode.Create(1);
        ueNode.Create(1);

        LteHelper lteHelper;

        lteHelper.Install(eNBNode);
        lteHelper.Install(ueNode);

        // Verify that LTE stack is installed on eNB and UE nodes
        Ptr<LteUeNetDevice> ueLteDevice = ueNode.Get(0)->GetObject<LteUeNetDevice>();
        Ptr<LteEnbNetDevice> enbLteDevice = eNBNode.Get(0)->GetObject<LteEnbNetDevice>();

        NS_TEST_ASSERT_MSG_NE(ueLteDevice, nullptr, "Failed to install LTE stack on UE node.");
        NS_TEST_ASSERT_MSG_NE(enbLteDevice, nullptr, "Failed to install LTE stack on eNB node.");
    }

    // Test IP address assignment (verify that IP addresses are assigned to both eNB and UE nodes)
    void TestIpv4AddressAssignment()
    {
        NodeContainer eNBNode, ueNode;
        eNBNode.Create(1);
        ueNode.Create(1);

        LteHelper lteHelper;
        InternetStackHelper internet;

        lteHelper.Install(eNBNode);
        lteHelper.Install(ueNode);

        Ipv4AddressHelper ipv4;
        Ipv4InterfaceContainer ueIpIface, enbIpIface;
        ueIpIface = lteHelper.AssignUeIpv4Address(ueNode.Get(0));
        enbIpIface = lteHelper.AssignUeIpv4Address(eNBNode.Get(0));

        // Verify that IP addresses are assigned correctly to eNB and UE
        NS_TEST_ASSERT_MSG_NE(ueIpIface.GetAddress(0), Ipv4Address("0.0.0.0"), "Failed to assign IP address to UE node.");
        NS_TEST_ASSERT_MSG_NE(enbIpIface.GetAddress(0), Ipv4Address("0.0.0.0"), "Failed to assign IP address to eNB node.");
    }

    // Test UDP server setup (verify that the UDP server is set up on eNB)
    void TestUdpServerSetup()
    {
        NodeContainer eNBNode;
        eNBNode.Create(1);

        UdpServerHelper udpServer(9);
        ApplicationContainer serverApps = udpServer.Install(eNBNode.Get(0));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        // Verify that the UDP server is installed on eNB node
        NS_TEST_ASSERT_MSG_EQ(serverApps.GetN(), 1, "Failed to install UDP server on eNB node.");
    }

    // Test UDP client setup (verify that the UDP client is set up on UE)
    void TestUdpClientSetup()
    {
        NodeContainer ueNode;
        ueNode.Create(1);

        Ipv4AddressHelper ipv4;
        Ipv4InterfaceContainer enbIpIface;
        enbIpIface.SetBase("10.1.1.0", "255.255.255.0");

        UdpClientHelper udpClient(enbIpIface.GetAddress(0), 9); // eNB as receiver
        udpClient.SetAttribute("MaxPackets", UintegerValue(5));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        udpClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps = udpClient.Install(ueNode.Get(0));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

        // Verify that the UDP client is installed on UE node
        NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), 1, "Failed to install UDP client on UE node.");
    }

    // Test data transmission (verify that data is transmitted from UE to eNB and received by server)
    void TestDataTransmission()
    {
        NodeContainer eNBNode, ueNode;
        eNBNode.Create(1);
        ueNode.Create(1);

        Ipv4AddressHelper ipv4;
        Ipv4InterfaceContainer ueIpIface, enbIpIface;
        LteHelper lteHelper;

        lteHelper.Install(eNBNode);
        lteHelper.Install(ueNode);

        ueIpIface = lteHelper.AssignUeIpv4Address(ueNode.Get(0));
        enbIpIface = lteHelper.AssignUeIpv4Address(eNBNode.Get(0));

        UdpServerHelper udpServer(9);
        ApplicationContainer serverApps = udpServer.Install(eNBNode.Get(0));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        UdpClientHelper udpClient(enbIpIface.GetAddress(0), 9); // eNB as receiver
        udpClient.SetAttribute("MaxPackets", UintegerValue(5));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        udpClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps = udpClient.Install(ueNode.Get(0));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

        Simulator::Run();
        Simulator::Destroy();

        // Verify if data was transmitted (not implemented here; you would need to check received packets)
    }
};

int main(int argc, char *argv[])
{
    LteTestSuite testSuite;
    testSuite.Run();
    return 0;
}
