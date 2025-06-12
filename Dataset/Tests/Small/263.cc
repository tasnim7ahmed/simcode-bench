#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class LteNetworkTestSuite : public TestCase
{
public:
    LteNetworkTestSuite() : TestCase("Test LTE Network Setup for Nodes") {}
    virtual ~LteNetworkTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestMobilityModel();
        TestLteDeviceInstallation();
        TestLteAttachment();
        TestIpAddressAssignment();
        TestUdpApplicationSetup();
    }

private:
    // Test node creation (ensure correct creation of eNodeB and UE)
    void TestNodeCreation()
    {
        NodeContainer enbNode;
        enbNode.Create(1); // Create 1 eNodeB
        NodeContainer ueNodes;
        ueNodes.Create(1); // Create 1 UE

        // Verify the number of nodes created
        NS_TEST_ASSERT_MSG_EQ(enbNode.GetN(), 1, "Failed to create eNodeB node.");
        NS_TEST_ASSERT_MSG_EQ(ueNodes.GetN(), 1, "Failed to create UE node.");
    }

    // Test the mobility configuration for both eNodeB and UE
    void TestMobilityModel()
    {
        NodeContainer enbNode;
        enbNode.Create(1);
        NodeContainer ueNodes;
        ueNodes.Create(1);

        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(enbNode);

        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
        mobility.Install(ueNodes);

        // Verify that the mobility model is set correctly for each node
        Ptr<MobilityModel> enbMobility = enbNode.Get(0)->GetObject<MobilityModel>();
        Ptr<MobilityModel> ueMobility = ueNodes.Get(0)->GetObject<MobilityModel>();

        NS_TEST_ASSERT_MSG_NE(enbMobility, nullptr, "Mobility model not installed on eNodeB.");
        NS_TEST_ASSERT_MSG_NE(ueMobility, nullptr, "Mobility model not installed on UE.");
    }

    // Test LTE device installation on eNodeB and UE
    void TestLteDeviceInstallation()
    {
        NodeContainer enbNode;
        enbNode.Create(1);
        NodeContainer ueNodes;
        ueNodes.Create(1);

        Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
        Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
        lteHelper->SetEpcHelper(epcHelper);

        NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNode);
        NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

        // Verify that LTE devices are installed on eNodeB and UE
        NS_TEST_ASSERT_MSG_EQ(enbDevs.GetN(), 1, "Failed to install LTE device on eNodeB.");
        NS_TEST_ASSERT_MSG_EQ(ueDevs.GetN(), 1, "Failed to install LTE device on UE.");
    }

    // Test LTE attachment between eNodeB and UE
    void TestLteAttachment()
    {
        NodeContainer enbNode;
        enbNode.Create(1);
        NodeContainer ueNodes;
        ueNodes.Create(1);

        Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
        Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
        lteHelper->SetEpcHelper(epcHelper);

        NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNode);
        NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

        // Attach the UE to the eNodeB
        lteHelper->Attach(ueDevs.Get(0), enbDevs.Get(0));

        // Verify that the UE is attached to the eNodeB (Check if attachment is successful)
        Ptr<LteEnbNetDevice> enbDevice = enbDevs.Get(0)->GetObject<LteEnbNetDevice>();
        Ptr<LteUeNetDevice> ueDevice = ueDevs.Get(0)->GetObject<LteUeNetDevice>();

        NS_TEST_ASSERT_MSG_NE(enbDevice, nullptr, "eNodeB LTE device is not installed.");
        NS_TEST_ASSERT_MSG_NE(ueDevice, nullptr, "UE LTE device is not installed.");
        NS_TEST_ASSERT_MSG_EQ(ueDevice->GetCellId(), enbDevice->GetCellId(), "UE is not attached to the eNodeB.");
    }

    // Test IP address assignment for the UE node
    void TestIpAddressAssignment()
    {
        NodeContainer enbNode;
        enbNode.Create(1);
        NodeContainer ueNodes;
        ueNodes.Create(1);

        Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
        Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
        lteHelper->SetEpcHelper(epcHelper);

        NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNode);
        NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer ueIpIface = address.Assign(ueDevs);

        // Verify that the UE has been assigned an IP address
        Ipv4Address ueIp = ueIpIface.GetAddress(0);
        NS_TEST_ASSERT_MSG_NE(ueIp, Ipv4Address("0.0.0.0"), "Failed to assign IP address to UE.");
    }

    // Test the setup of UDP Echo Server and Client applications
    void TestUdpApplicationSetup()
    {
        NodeContainer enbNode;
        enbNode.Create(1);
        NodeContainer ueNodes;
        ueNodes.Create(1);

        Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
        Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
        lteHelper->SetEpcHelper(epcHelper);

        NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNode);
        NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer ueIpIface = address.Assign(ueDevs);

        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApps = echoServer.Install(ueNodes.Get(0)); // Install on the UE node
        serverApps.Start(Seconds(2.0));
        serverApps.Stop(Seconds(10.0));

        UdpEchoClientHelper echoClient(ueIpIface.GetAddress(0), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(10));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps = echoClient.Install(enbNode.Get(0)); // Install on the eNodeB node
        clientApps.Start(Seconds(3.0));
        clientApps.Stop(Seconds(10.0));

        // Verify that the server and client applications are correctly installed
        NS_TEST_ASSERT_MSG_EQ(serverApps.GetN(), 1, "Failed to install UDP Echo Server on UE.");
        NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), 1, "Failed to install UDP Echo Client on eNodeB.");
    }
};

// Instantiate the test suite
static LteNetworkTestSuite lteNetworkTestSuite;
