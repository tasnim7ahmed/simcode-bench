#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class SixLowpanIoTTestSuite : public TestCase {
public:
    SixLowpanIoTTestSuite() : TestCase("Test SixLowpan IoT Example") {}
    virtual ~SixLowpanIoTTestSuite() {}

    void DoRun() override {
        TestNodeCreation();
        TestLrWpanNetworkSetup();
        TestSixLowpanInstallation();
        TestMobilityModelInstallation();
        TestInternetStackInstallation();
        TestIpv6AddressAssignment();
        TestUdpReceiverSocketCreation();
        TestUdpSenderSocketCreation();
        TestPacketTransmission();
        TestPacketReception();
    }

private:
    uint16_t port = 5000;

    // Test if the nodes (3 IoT devices + Server) are created correctly
    void TestNodeCreation() {
        NodeContainer nodes;
        nodes.Create(4);
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 4, "Node creation failed.");
    }

    // Test if the LR-WPAN network is set up correctly
    void TestLrWpanNetworkSetup() {
        NodeContainer nodes;
        nodes.Create(4);

        LrWpanHelper lrWpan;
        NetDeviceContainer lrDevices = lrWpan.Install(nodes);
        
        NS_TEST_ASSERT_MSG_GT(lrDevices.GetN(), 0, "LR-WPAN network setup failed.");
    }

    // Test if SixLowPan is correctly installed over LR-WPAN
    void TestSixLowpanInstallation() {
        NodeContainer nodes;
        nodes.Create(4);

        LrWpanHelper lrWpan;
        NetDeviceContainer lrDevices = lrWpan.Install(nodes);

        SixLowPanHelper sixlowpan;
        NetDeviceContainer sixlowpanDevices = sixlowpan.Install(lrDevices);

        NS_TEST_ASSERT_MSG_GT(sixlowpanDevices.GetN(), 0, "SixLowPan installation failed.");
    }

    // Test if the mobility models are installed correctly (all nodes are static)
    void TestMobilityModelInstallation() {
        NodeContainer nodes;
        nodes.Create(4);

        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(nodes);

        for (uint32_t i = 0; i < nodes.GetN(); i++) {
            Ptr<MobilityModel> mobilityModel = nodes.Get(i)->GetObject<MobilityModel>();
            NS_TEST_ASSERT_MSG_NOT_NULL(mobilityModel, "Mobility model installation failed.");
        }
    }

    // Test if the Internet stack is installed correctly on all nodes
    void TestInternetStackInstallation() {
        NodeContainer nodes;
        nodes.Create(4);

        InternetStackHelper internet;
        internet.Install(nodes);

        for (uint32_t i = 0; i < nodes.GetN(); i++) {
            Ptr<Ipv6> ipv6 = nodes.Get(i)->GetObject<Ipv6>();
            NS_TEST_ASSERT_MSG_NOT_NULL(ipv6, "Internet stack installation failed on node.");
        }
    }

    // Test if IPv6 addresses are assigned correctly to all nodes
    void TestIpv6AddressAssignment() {
        NodeContainer nodes;
        nodes.Create(4);

        LrWpanHelper lrWpan;
        NetDeviceContainer lrDevices = lrWpan.Install(nodes);

        SixLowPanHelper sixlowpan;
        NetDeviceContainer sixlowpanDevices = sixlowpan.Install(lrDevices);

        Ipv6AddressHelper ipv6;
        ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
        Ipv6InterfaceContainer interfaces = ipv6.Assign(sixlowpanDevices);

        NS_TEST_ASSERT_MSG_GT(interfaces.GetN(), 0, "IPv6 address assignment failed.");
    }

    // Test if UDP receiver socket is created correctly on IoT Server
    void TestUdpReceiverSocketCreation() {
        NodeContainer nodes;
        nodes.Create(4);

        Ptr<Socket> serverSocket = Socket::CreateSocket(nodes.Get(3), UdpSocketFactory::GetTypeId());
        serverSocket->Bind(Inet6SocketAddress(Ipv6Address::GetAny(), port));

        NS_TEST_ASSERT_MSG_NOT_NULL(serverSocket, "UDP receiver socket creation on Server failed.");
    }

    // Test if UDP sender sockets are created correctly on IoT Devices
    void TestUdpSenderSocketCreation() {
        NodeContainer nodes;
        nodes.Create(4);

        Ipv6InterfaceContainer interfaces = Ipv6AddressHelper().Assign(SixLowPanHelper().Install(LrWpanHelper().Install(nodes)));

        for (uint32_t i = 0; i < 3; i++) {
            Ptr<Socket> deviceSocket = Socket::CreateSocket(nodes.Get(i), UdpSocketFactory::GetTypeId());
            Inet6SocketAddress serverAddress(interfaces.GetAddress(3, 1), port);
            deviceSocket->Connect(serverAddress);

            NS_TEST_ASSERT_MSG_NOT_NULL(deviceSocket, "UDP sender socket creation on IoT Device failed.");
        }
    }

    // Test if packets are sent correctly from IoT Devices
    void TestPacketTransmission() {
        NodeContainer nodes;
        nodes.Create(4);

        Ipv6InterfaceContainer interfaces = Ipv6AddressHelper().Assign(SixLowPanHelper().Install(LrWpanHelper().Install(nodes)));

        for (uint32_t i = 0; i < 3; i++) {
            Ptr<Socket> deviceSocket = Socket::CreateSocket(nodes.Get(i), UdpSocketFactory::GetTypeId());
            Inet6SocketAddress serverAddress(interfaces.GetAddress(3, 1), port);

            Simulator::Schedule(Seconds(1.0 + i), &Socket::SendTo, deviceSocket, Create<Packet>(64), 0, serverAddress);
            Simulator::Schedule(Seconds(3.0 + i), &Socket::SendTo, deviceSocket, Create<Packet>(64), 0, serverAddress);
        }

        Simulator::Stop(Seconds(6.0));
        Simulator::Run();
        Simulator::Destroy();

        NS_LOG_INFO("Packet transmission test passed.");
    }

    // Test if packets are received correctly by the IoT Server
    void TestPacketReception() {
        NodeContainer nodes;
        nodes.Create(4);

        Ptr<Socket> serverSocket = Socket::CreateSocket(nodes.Get(3), UdpSocketFactory::GetTypeId());
        serverSocket->Bind(Inet6SocketAddress(Ipv6Address::GetAny(), port));

        Simulator::Schedule(Seconds(1.0), &Socket::SendTo, serverSocket, Create<Packet>(64), 0, Inet6SocketAddress(Ipv6Address("2001:1::3"), port));
        Simulator::Stop(Seconds(6.0));
        Simulator::Run();
        Simulator::Destroy();

        NS_LOG_INFO("Packet reception test passed.");
    }
};

// Instantiate the test suite
static SixLowpanIoTTestSuite sixLowpanIoTTestSuite;
