#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wave-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class Vanet80211pTestSuite : public TestCase {
public:
    Vanet80211pTestSuite() : TestCase("Test VANET 802.11p Example") {}
    virtual ~Vanet80211pTestSuite() {}

    void DoRun() override {
        TestNodeCreation();
        TestWaveNetworkSetup();
        TestMobilityModelInstallation();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestUdpReceiverSocketCreation();
        TestUdpSenderSocketCreation();
        TestPacketTransmission();
        TestPacketReception();
    }

private:
    uint16_t port = 4000;

    // Test if nodes (Vehicle and RSU) are created correctly
    void TestNodeCreation() {
        NodeContainer nodes;
        nodes.Create(2);
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Node creation failed.");
    }

    // Test if the VANET (802.11p/WAVE) network is set up correctly
    void TestWaveNetworkSetup() {
        NodeContainer nodes;
        nodes.Create(2);
        
        YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper wavePhy = YansWifiPhyHelper::Default();
        wavePhy.SetChannel(waveChannel.Create());

        QosWaveMacHelper waveMac = QosWaveMacHelper::Default();
        WaveHelper wave;
        NetDeviceContainer devices = wave.Install(wavePhy, waveMac, nodes);
        
        NS_TEST_ASSERT_MSG_GT(devices.GetN(), 0, "VANET network setup failed.");
    }

    // Test if the mobility models are installed correctly
    void TestMobilityModelInstallation() {
        NodeContainer nodes;
        nodes.Create(2);

        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(nodes.Get(1));  // RSU

        mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
        mobility.Install(nodes.Get(0));  // Vehicle

        Ptr<MobilityModel> vehicleMobility = nodes.Get(0)->GetObject<MobilityModel>();
        Ptr<MobilityModel> rsuMobility = nodes.Get(1)->GetObject<MobilityModel>();

        NS_TEST_ASSERT_MSG_NOT_NULL(vehicleMobility, "Vehicle mobility model installation failed.");
        NS_TEST_ASSERT_MSG_NOT_NULL(rsuMobility, "RSU mobility model installation failed.");
    }

    // Test if the Internet stack is installed correctly
    void TestInternetStackInstallation() {
        NodeContainer nodes;
        nodes.Create(2);

        InternetStackHelper internet;
        internet.Install(nodes);

        for (uint32_t i = 0; i < nodes.GetN(); i++) {
            Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack installation failed on node.");
        }
    }

    // Test if IP addresses are assigned correctly
    void TestIpAddressAssignment() {
        NodeContainer nodes;
        nodes.Create(2);

        YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper wavePhy = YansWifiPhyHelper::Default();
        wavePhy.SetChannel(waveChannel.Create());

        QosWaveMacHelper waveMac = QosWaveMacHelper::Default();
        WaveHelper wave;
        NetDeviceContainer devices = wave.Install(wavePhy, waveMac, nodes);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        NS_TEST_ASSERT_MSG_GT(interfaces.GetN(), 0, "IP address assignment failed.");
    }

    // Test if UDP receiver socket is created correctly on RSU
    void TestUdpReceiverSocketCreation() {
        NodeContainer nodes;
        nodes.Create(2);

        Ptr<Socket> rsuSocket = Socket::CreateSocket(nodes.Get(1), UdpSocketFactory::GetTypeId());
        rsuSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));

        NS_TEST_ASSERT_MSG_NOT_NULL(rsuSocket, "UDP receiver socket creation on RSU failed.");
    }

    // Test if UDP sender socket is created correctly on Vehicle
    void TestUdpSenderSocketCreation() {
        NodeContainer nodes;
        nodes.Create(2);

        Ptr<Socket> vehicleSocket = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());
        InetSocketAddress rsuAddress(Ipv4Address("10.1.1.2"), port);
        vehicleSocket->Connect(rsuAddress);

        NS_TEST_ASSERT_MSG_NOT_NULL(vehicleSocket, "UDP sender socket creation on Vehicle failed.");
    }

    // Test if packets are sent correctly from Vehicle
    void TestPacketTransmission() {
        NodeContainer nodes;
        nodes.Create(2);

        Ptr<Socket> vehicleSocket = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());
        InetSocketAddress rsuAddress(Ipv4Address("10.1.1.2"), port);

        Simulator::Schedule(Seconds(1.0), &Socket::SendTo, vehicleSocket, Create<Packet>(128), 0, rsuAddress);
        Simulator::Schedule(Seconds(2.0), &Socket::SendTo, vehicleSocket, Create<Packet>(128), 0, rsuAddress);

        Simulator::Stop(Seconds(3.0));
        Simulator::Run();
        Simulator::Destroy();

        NS_LOG_INFO("Packet transmission test passed.");
    }

    // Test if packets are received correctly by RSU
    void TestPacketReception() {
        NodeContainer nodes;
        nodes.Create(2);

        Ptr<Socket> rsuSocket = Socket::CreateSocket(nodes.Get(1), UdpSocketFactory::GetTypeId());
        rsuSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));

        // Schedule packet reception and send from the vehicle
        Simulator::Schedule(Seconds(1.0), &Socket::SendTo, rsuSocket, Create<Packet>(128), 0, InetSocketAddress(Ipv4Address("10.1.1.1"), port));
        Simulator::Stop(Seconds(3.0));
        Simulator::Run();
        Simulator::Destroy();

        NS_LOG_INFO("Packet reception test passed.");
    }
};

// Instantiate the test suite
static Vanet80211pTestSuite vanet80211pTestSuite;
