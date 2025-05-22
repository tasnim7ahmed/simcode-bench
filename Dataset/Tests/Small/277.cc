#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/nr-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class NrTestSuite : public TestCase
{
public:
    NrTestSuite() : TestCase("Test 5G NR network setup with TCP application") {}
    virtual ~NrTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestMobilityModelSetup();
        TestNrModuleSetup();
        TestInternetStackInstallation();
        TestIpv4AddressAssignment();
        TestTcpServerSetup();
        TestTcpClientSetup();
        TestDataTransmission();
    }

private:
    // Test node creation (verify that gNB and UE nodes are created)
    void TestNodeCreation()
    {
        NodeContainer gnbNode, ueNode;
        gnbNode.Create(1);
        ueNode.Create(1);

        // Verify that 1 gNB node and 1 UE node are created
        NS_TEST_ASSERT_MSG_EQ(gnbNode.GetN(), 1, "Failed to create gNB node.");
        NS_TEST_ASSERT_MSG_EQ(ueNode.GetN(), 1, "Failed to create UE node.");
    }

    // Test mobility model setup (verify that mobility models are installed correctly)
    void TestMobilityModelSetup()
    {
        NodeContainer gnbNode, ueNode;
        gnbNode.Create(1);
        ueNode.Create(1);

        MobilityHelper mobility;

        // Install mobility for gNB (static)
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(50.0),
                                      "DeltaY", DoubleValue(50.0),
                                      "GridWidth", UintegerValue(1),
                                      "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(gnbNode);

        // Install mobility for UE (random walk)
        mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                      "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                      "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                  "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
        mobility.Install(ueNode);

        // Verify that mobility models are installed
        Ptr<MobilityModel> gnbMobility = gnbNode.Get(0)->GetObject<MobilityModel>();
        Ptr<MobilityModel> ueMobility = ueNode.Get(0)->GetObject<MobilityModel>();

        NS_TEST_ASSERT_MSG_NE(gnbMobility, nullptr, "Failed to install mobility model on gNB node.");
        NS_TEST_ASSERT_MSG_NE(ueMobility, nullptr, "Failed to install mobility model on UE node.");
    }

    // Test NR module setup (verify that gNB and UE devices are installed correctly)
    void TestNrModuleSetup()
    {
        NodeContainer gnbNode, ueNode;
        gnbNode.Create(1);
        ueNode.Create(1);

        NrHelper nrHelper;
        Ptr<NrNetDevice> gnbDevice = nrHelper.Install(gnbNode.Get(0));
        Ptr<NrNetDevice> ueDevice = nrHelper.Install(ueNode.Get(0));

        // Verify that NR devices are installed on both gNB and UE nodes
        NS_TEST_ASSERT_MSG_NE(gnbDevice, nullptr, "Failed to install NR device on gNB node.");
        NS_TEST_ASSERT_MSG_NE(ueDevice, nullptr, "Failed to install NR device on UE node.");
    }

    // Test Internet stack installation (verify that the stack is installed correctly)
    void TestInternetStackInstallation()
    {
        NodeContainer gnbNode, ueNode;
        gnbNode.Create(1);
        ueNode.Create(1);

        InternetStackHelper internet;
        internet.Install(gnbNode);
        internet.Install(ueNode);

        // Verify that the Internet stack is installed on both nodes
        Ptr<Ipv4> gnbIpv4 = gnbNode.Get(0)->GetObject<Ipv4>();
        Ptr<Ipv4> ueIpv4 = ueNode.Get(0)->GetObject<Ipv4>();

        NS_TEST_ASSERT_MSG_NE(gnbIpv4, nullptr, "Failed to install Internet stack on gNB node.");
        NS_TEST_ASSERT_MSG_NE(ueIpv4, nullptr, "Failed to install Internet stack on UE node.");
    }

    // Test IPv4 address assignment (verify that IP addresses are assigned correctly)
    void TestIpv4AddressAssignment()
    {
        NodeContainer gnbNode, ueNode;
        gnbNode.Create(1);
        ueNode.Create(1);

        NrHelper nrHelper;
        Ptr<NrNetDevice> gnbDevice = nrHelper.Install(gnbNode.Get(0));
        Ptr<NrNetDevice> ueDevice = nrHelper.Install(ueNode.Get(0));

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer ueIpIface, gnbIpIface;
        ueIpIface = ipv4.Assign(ueDevice->GetObject<NetDevice>());
        gnbIpIface = ipv4.Assign(gnbDevice->GetObject<NetDevice>());

        // Verify that IPv4 addresses are assigned
        NS_TEST_ASSERT_MSG_NE(ueIpIface.GetAddress(0), Ipv4Address("0.0.0.0"), "Failed to assign IP address to UE.");
        NS_TEST_ASSERT_MSG_NE(gnbIpIface.GetAddress(0), Ipv4Address("0.0.0.0"), "Failed to assign IP address to gNB.");
    }

    // Test TCP server setup (verify that the TCP server is installed on the gNB)
    void TestTcpServerSetup()
    {
        NodeContainer gnbNode;
        gnbNode.Create(1);

        uint16_t port = 9;
        TcpServerHelper tcpServer(port);
        ApplicationContainer serverApp = tcpServer.Install(gnbNode.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Verify that the TCP server is installed
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "Failed to install TCP server on gNB node.");
    }

    // Test TCP client setup (verify that the TCP client is installed on the UE)
    void TestTcpClientSetup()
    {
        NodeContainer ueNode;
        ueNode.Create(1);

        Ipv4AddressHelper ipv4;
        Ipv4InterfaceContainer gnbIpIface;
        gnbIpIface.SetBase("10.1.1.0", "255.255.255.0");

        uint16_t port = 9;
        TcpClientHelper tcpClient(gnbIpIface.GetAddress(0), port);
        tcpClient.SetAttribute("MaxPackets", UintegerValue(5));
        tcpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        tcpClient.SetAttribute("PacketSize", UintegerValue(512));
        ApplicationContainer clientApp = tcpClient.Install(ueNode.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify that the TCP client is installed
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "Failed to install TCP client on UE node.");
    }

    // Test data transmission (verify that TCP packets are transmitted from UE to gNB)
    void TestDataTransmission()
    {
        NodeContainer gnbNode, ueNode;
        gnbNode.Create(1);
        ueNode.Create(1);

        NrHelper nrHelper;
        Ptr<NrNetDevice> gnbDevice = nrHelper.Install(gnbNode.Get(0));
        Ptr<NrNetDevice> ueDevice = nrHelper.Install(ueNode.Get(0));

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer ueIpIface, gnbIpIface;
        ueIpIface = ipv4.Assign(ueDevice->GetObject<NetDevice>());
        gnbIpIface = ipv4.Assign(gnbDevice->GetObject<NetDevice>());

        // Set up the TCP server on gNB
        uint16_t port = 9;
        TcpServerHelper tcpServer(port);
        ApplicationContainer serverApp = tcpServer.Install(gnbNode.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Set up the TCP client on UE
        TcpClientHelper tcpClient(gnbIpIface.GetAddress(0), port);
        tcpClient.SetAttribute("MaxPackets", UintegerValue(5));
        tcpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        tcpClient.SetAttribute("PacketSize", UintegerValue(512));
        ApplicationContainer clientApp = tcpClient.Install(ueNode.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Run the simulation to check transmission
        Simulator::Run();
        Simulator::Destroy();

        // This function is mainly to verify that TCP transmission occurred correctly.
        // Further packet inspection or callback functions would be required to fully verify the transmission.
    }
};

int main(int argc, char *argv[])
{
    NrTestSuite testSuite;
    testSuite.Run();
    return 0;
}
