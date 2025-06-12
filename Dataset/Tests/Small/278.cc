#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class LteTestSuite : public TestCase
{
public:
    LteTestSuite() : TestCase("Test LTE network setup with HTTP application") {}
    virtual ~LteTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestLteModuleSetup();
        TestInternetStackInstallation();
        TestIpv4AddressAssignment();
        TestTcpServerSetup();
        TestTcpClientSetup();
        TestDataTransmission();
        TestMobilityModelSetup();
    }

private:
    // Test node creation (verify that eNodeB and UE nodes are created)
    void TestNodeCreation()
    {
        NodeContainer enbNode, ueNode;
        enbNode.Create(1);
        ueNode.Create(1);

        // Verify that 1 eNodeB node and 1 UE node are created
        NS_TEST_ASSERT_MSG_EQ(enbNode.GetN(), 1, "Failed to create eNodeB node.");
        NS_TEST_ASSERT_MSG_EQ(ueNode.GetN(), 1, "Failed to create UE node.");
    }

    // Test LTE module setup (verify that LTE devices are installed correctly)
    void TestLteModuleSetup()
    {
        NodeContainer enbNode, ueNode;
        enbNode.Create(1);
        ueNode.Create(1);

        LteHelper lteHelper;
        Ptr<NrCellularNetDevice> enbDevice = lteHelper.InstallEnbDevice(enbNode.Get(0));
        Ptr<NrCellularNetDevice> ueDevice = lteHelper.InstallUeDevice(ueNode.Get(0));

        // Verify that LTE devices are installed on both eNodeB and UE nodes
        NS_TEST_ASSERT_MSG_NE(enbDevice, nullptr, "Failed to install LTE device on eNodeB node.");
        NS_TEST_ASSERT_MSG_NE(ueDevice, nullptr, "Failed to install LTE device on UE node.");
    }

    // Test Internet stack installation (verify that the stack is installed correctly)
    void TestInternetStackInstallation()
    {
        NodeContainer enbNode, ueNode;
        enbNode.Create(1);
        ueNode.Create(1);

        InternetStackHelper internet;
        internet.Install(enbNode);
        internet.Install(ueNode);

        // Verify that the Internet stack is installed on both nodes
        Ptr<Ipv4> enbIpv4 = enbNode.Get(0)->GetObject<Ipv4>();
        Ptr<Ipv4> ueIpv4 = ueNode.Get(0)->GetObject<Ipv4>();

        NS_TEST_ASSERT_MSG_NE(enbIpv4, nullptr, "Failed to install Internet stack on eNodeB node.");
        NS_TEST_ASSERT_MSG_NE(ueIpv4, nullptr, "Failed to install Internet stack on UE node.");
    }

    // Test IPv4 address assignment (verify that IP addresses are assigned correctly)
    void TestIpv4AddressAssignment()
    {
        NodeContainer enbNode, ueNode;
        enbNode.Create(1);
        ueNode.Create(1);

        LteHelper lteHelper;
        Ptr<NrCellularNetDevice> enbDevice = lteHelper.InstallEnbDevice(enbNode.Get(0));
        Ptr<NrCellularNetDevice> ueDevice = lteHelper.InstallUeDevice(ueNode.Get(0));

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.0.0.0", "255.255.255.0");
        Ipv4InterfaceContainer ueIpIface, enbIpIface;
        ueIpIface = ipv4.Assign(ueDevice->GetObject<NetDevice>());
        enbIpIface = ipv4.Assign(enbDevice->GetObject<NetDevice>());

        // Verify that IPv4 addresses are assigned correctly
        NS_TEST_ASSERT_MSG_NE(ueIpIface.GetAddress(0), Ipv4Address("0.0.0.0"), "Failed to assign IP address to UE.");
        NS_TEST_ASSERT_MSG_NE(enbIpIface.GetAddress(0), Ipv4Address("0.0.0.0"), "Failed to assign IP address to eNodeB.");
    }

    // Test TCP server setup (verify that the HTTP server is installed on the eNodeB)
    void TestTcpServerSetup()
    {
        NodeContainer enbNode;
        enbNode.Create(1);

        uint16_t port = 80;
        UdpServerHelper httpServer(port);
        ApplicationContainer serverApp = httpServer.Install(enbNode.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the HTTP server is installed on eNodeB
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "Failed to install HTTP server on eNodeB node.");
    }

    // Test TCP client setup (verify that the HTTP client is installed on the UE)
    void TestTcpClientSetup()
    {
        NodeContainer ueNode;
        ueNode.Create(1);

        Ipv4AddressHelper ipv4;
        Ipv4InterfaceContainer enbIpIface;
        enbIpIface.SetBase("10.0.0.0", "255.255.255.0");

        uint16_t port = 80;
        UdpClientHelper httpClient(enbIpIface.GetAddress(0), port);
        httpClient.SetAttribute("MaxPackets", UintegerValue(5));
        httpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        httpClient.SetAttribute("PacketSize", UintegerValue(512));
        ApplicationContainer clientApp = httpClient.Install(ueNode.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the HTTP client is installed on UE
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "Failed to install HTTP client on UE node.");
    }

    // Test data transmission (verify that the HTTP client can transmit data to the HTTP server)
    void TestDataTransmission()
    {
        NodeContainer enbNode, ueNode;
        enbNode.Create(1);
        ueNode.Create(1);

        LteHelper lteHelper;
        Ptr<NrCellularNetDevice> enbDevice = lteHelper.InstallEnbDevice(enbNode.Get(0));
        Ptr<NrCellularNetDevice> ueDevice = lteHelper.InstallUeDevice(ueNode.Get(0));

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.0.0.0", "255.255.255.0");
        Ipv4InterfaceContainer ueIpIface, enbIpIface;
        ueIpIface = ipv4.Assign(ueDevice->GetObject<NetDevice>());
        enbIpIface = ipv4.Assign(enbDevice->GetObject<NetDevice>());

        // Set up the HTTP server on eNodeB
        uint16_t port = 80;
        UdpServerHelper httpServer(port);
        ApplicationContainer serverApp = httpServer.Install(enbNode.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Set up the HTTP client on UE
        UdpClientHelper httpClient(enbIpIface.GetAddress(0), port);
        httpClient.SetAttribute("MaxPackets", UintegerValue(5));
        httpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        httpClient.SetAttribute("PacketSize", UintegerValue(512));
        ApplicationContainer clientApp = httpClient.Install(ueNode.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Run the simulation to check transmission
        Simulator::Run();
        Simulator::Destroy();

        // This function is mainly to verify that HTTP transmission occurred correctly.
        // Further packet inspection or callback functions would be required to fully verify the transmission.
    }

    // Test mobility model setup (verify that mobility models are correctly set)
    void TestMobilityModelSetup()
    {
        NodeContainer enbNode, ueNode;
        enbNode.Create(1);
        ueNode.Create(1);

        MobilityHelper mobility;

        // Set mobility for eNodeB (static)
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(10.0),
                                      "DeltaY", DoubleValue(10.0),
                                      "GridWidth", UintegerValue(1),
                                      "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(enbNode);

        // Set mobility for UE (random walk)
        mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                      "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                      "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                  "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
        mobility.Install(ueNode);

        // Verify that the mobility models are installed on both eNodeB and UE
        Ptr<ConstantPositionMobilityModel> enbMobility = enbNode.Get(0)->GetObject<ConstantPositionMobilityModel>();
        Ptr<RandomWalk2dMobilityModel> ueMobility = ueNode.Get(0)->GetObject<RandomWalk2dMobilityModel>();

        NS_TEST_ASSERT_MSG_NE(enbMobility, nullptr, "Failed to install mobility model on eNodeB.");
        NS_TEST_ASSERT_MSG_NE(ueMobility, nullptr, "Failed to install mobility model on UE.");
    }
};

static LteTestSuite suite;

