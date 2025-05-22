#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

// Unit Test Suite for LteMobileNetworkExample
class LteMobileNetworkTest : public TestCase
{
public:
    LteMobileNetworkTest() : TestCase("Test for LteMobileNetworkExample") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestLteDeviceInstallation();
        TestMobilityModelInstallation();
        TestIpv4AddressAssignment();
        TestUdpApplicationSetup();
        TestSimulationExecution();
    }

private:
    uint32_t nEnbNodes = 1;
    uint32_t nUeNodes = 1;
    NodeContainer eNodeB, ue;
    NetDeviceContainer enbLteDevs, ueLteDevs;
    Ipv4InterfaceContainer ueIpIface, enbIpIface;

    // Test for node creation
    void TestNodeCreation()
    {
        eNodeB.Create(nEnbNodes);
        ue.Create(nUeNodes);

        // Check that nodes are created successfully
        NS_TEST_ASSERT_MSG_EQ(eNodeB.GetN(), nEnbNodes, "Failed to create eNodeB node");
        NS_TEST_ASSERT_MSG_EQ(ue.GetN(), nUeNodes, "Failed to create UE node");
    }

    // Test for LTE device installation
    void TestLteDeviceInstallation()
    {
        LteHelper lte;
        InternetStackHelper internet;

        // Install LTE devices on the nodes
        enbLteDevs = lte.InstallEnbDevice(eNodeB);
        ueLteDevs = lte.InstallUeDevice(ue);

        // Install Internet stack
        internet.Install(eNodeB);
        internet.Install(ue);

        // Verify that LTE devices are installed
        NS_TEST_ASSERT_MSG_EQ(enbLteDevs.GetN(), nEnbNodes, "Failed to install eNodeB LTE devices");
        NS_TEST_ASSERT_MSG_EQ(ueLteDevs.GetN(), nUeNodes, "Failed to install UE LTE devices");
    }

    // Test for mobility model installation
    void TestMobilityModelInstallation()
    {
        MobilityHelper mobility;

        // Set mobility model for UE (RandomWalk2dMobilityModel)
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(10.0),
                                      "DeltaY", DoubleValue(10.0),
                                      "GridWidth", UintegerValue(2),
                                      "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                  "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
        mobility.Install(ue);

        // Set constant position model for eNodeB
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(eNodeB);

        // Verify mobility model installation
        Ptr<MobilityModel> ueMobility = ue.Get(0)->GetObject<MobilityModel>();
        Ptr<MobilityModel> enbMobility = eNodeB.Get(0)->GetObject<MobilityModel>();

        NS_TEST_ASSERT_MSG_NE(ueMobility, nullptr, "Mobility model not installed on UE node");
        NS_TEST_ASSERT_MSG_NE(enbMobility, nullptr, "Mobility model not installed on eNodeB node");
    }

    // Test for IP address assignment
    void TestIpv4AddressAssignment()
    {
        Ipv4AddressHelper ipv4Address;
        ipv4Address.SetBase("7.0.0.0", "255.255.255.0");

        // Assign IP addresses to LTE devices
        ueIpIface = ipv4Address.Assign(ueLteDevs);
        enbIpIface = ipv4Address.Assign(enbLteDevs);

        // Verify that IP addresses are assigned
        Ipv4Address ueIp = ueIpIface.GetAddress(0);
        Ipv4Address enbIp = enbIpIface.GetAddress(0);

        NS_TEST_ASSERT_MSG_NE(ueIp, Ipv4Address("0.0.0.0"), "Invalid IP address assigned to UE");
        NS_TEST_ASSERT_MSG_NE(enbIp, Ipv4Address("0.0.0.0"), "Invalid IP address assigned to eNodeB");
    }

    // Test for UDP client-server application setup
    void TestUdpApplicationSetup()
    {
        uint16_t port = 1234;

        // Set up UDP client application on eNodeB
        UdpClientHelper udpClient(ueIpIface.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(100));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = udpClient.Install(eNodeB.Get(0));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the UDP client application is installed
        NS_TEST_ASSERT_MSG_GT(clientApp.GetN(), 0, "UDP client application not installed on eNodeB");

        // Set up UDP server application on UE
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(ue.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the UDP server application is installed
        NS_TEST_ASSERT_MSG_GT(serverApp.GetN(), 0, "UDP server application not installed on UE");
    }

    // Test for simulation execution
    void TestSimulationExecution()
    {
        Simulator::Run();
        Simulator::Destroy();

        // Verify that the simulation ran and was destroyed without errors
        NS_TEST_ASSERT_MSG_EQ(Simulator::GetContext(), 0, "Simulation encountered an error");
    }
};

// Register the test
static LteMobileNetworkTest lteMobileNetworkTestInstance;
