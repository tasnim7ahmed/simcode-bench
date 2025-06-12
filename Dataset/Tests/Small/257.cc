#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

class LteTestSuite : public TestCase
{
public:
    LteTestSuite() : TestCase("Test LTE Echo Application Setup") {}
    virtual ~LteTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestMobilityConfiguration();
        TestLteDeviceInstallation();
        TestIpAddressAssignment();
        TestApplicationSetup();
    }

private:
    // Test the creation of LTE nodes (UEs and eNB)
    void TestNodeCreation()
    {
        Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
        Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
        lteHelper->SetEpcHelper(epcHelper);

        NodeContainer ueNodes, enbNodes;
        ueNodes.Create(2);  // Two UEs (User Devices)
        enbNodes.Create(1); // One eNodeB (Base Station)

        // Ensure that the correct number of nodes are created
        NS_TEST_ASSERT_MSG_EQ(ueNodes.GetN(), 2, "Failed to create 2 UE nodes.");
        NS_TEST_ASSERT_MSG_EQ(enbNodes.GetN(), 1, "Failed to create 1 eNB node.");
    }

    // Test the mobility configuration of the nodes
    void TestMobilityConfiguration()
    {
        Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
        Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
        lteHelper->SetEpcHelper(epcHelper);

        NodeContainer ueNodes, enbNodes;
        ueNodes.Create(2);
        enbNodes.Create(1);

        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(enbNodes);

        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(5.0),
                                      "DeltaY", DoubleValue(0.0),
                                      "GridWidth", UintegerValue(1),
                                      "LayoutType", StringValue("RowFirst"));
        mobility.Install(ueNodes);

        // Ensure that mobility models have been set up
        Ptr<MobilityModel> mobilityModelEnb = enbNodes.Get(0)->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_NE(mobilityModelEnb, nullptr, "Mobility model is not set for eNB.");
        Ptr<MobilityModel> mobilityModelUe = ueNodes.Get(0)->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_NE(mobilityModelUe, nullptr, "Mobility model is not set for UE.");
    }

    // Test LTE device installation on eNB and UEs
    void TestLteDeviceInstallation()
    {
        Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
        Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
        lteHelper->SetEpcHelper(epcHelper);

        NodeContainer ueNodes, enbNodes;
        ueNodes.Create(2);
        enbNodes.Create(1);

        NetDeviceContainer enbLteDevices = lteHelper->InstallEnbDevice(enbNodes);
        NetDeviceContainer ueLteDevices = lteHelper->InstallUeDevice(ueNodes);

        // Ensure LTE devices are installed
        NS_TEST_ASSERT_MSG_EQ(enbLteDevices.GetN(), 1, "Failed to install LTE device on eNB.");
        NS_TEST_ASSERT_MSG_EQ(ueLteDevices.GetN(), 2, "Failed to install LTE devices on UEs.");
    }

    // Test IP address assignment for UEs
    void TestIpAddressAssignment()
    {
        Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
        Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
        lteHelper->SetEpcHelper(epcHelper);

        NodeContainer ueNodes, enbNodes;
        ueNodes.Create(2);
        enbNodes.Create(1);

        InternetStackHelper internet;
        internet.Install(ueNodes);

        NetDeviceContainer ueLteDevices = lteHelper->InstallUeDevice(ueNodes);
        Ipv4InterfaceContainer ueIpInterfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevices));

        // Verify that IP addresses are assigned
        Ipv4Address ipNode1 = ueIpInterfaces.GetAddress(0);
        Ipv4Address ipNode2 = ueIpInterfaces.GetAddress(1);

        NS_TEST_ASSERT_MSG_NE(ipNode1, Ipv4Address("0.0.0.0"), "Failed to assign IP address to UE 1.");
        NS_TEST_ASSERT_MSG_NE(ipNode2, Ipv4Address("0.0.0.0"), "Failed to assign IP address to UE 2.");
    }

    // Test the setup of the UDP Echo Server and Client applications
    void TestApplicationSetup()
    {
        Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
        Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
        lteHelper->SetEpcHelper(epcHelper);

        NodeContainer ueNodes, enbNodes;
        ueNodes.Create(2);
        enbNodes.Create(1);

        InternetStackHelper internet;
        internet.Install(ueNodes);

        Ipv4InterfaceContainer ueIpInterfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueNodes));

        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApps = echoServer.Install(ueNodes.Get(1)); // Second UE is the server
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        UdpEchoClientHelper echoClient(ueIpInterfaces.GetAddress(1), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(2));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps = echoClient.Install(ueNodes.Get(0)); // First UE is the client
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

        // Ensure that the server and client applications are correctly installed
        NS_TEST_ASSERT_MSG_EQ(serverApps.GetN(), 1, "Failed to install UDP Echo Server on UE 2.");
        NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), 1, "Failed to install UDP Echo Client on UE 1.");
    }
};

// Instantiate the test suite
static LteTestSuite lteTestSuite;
