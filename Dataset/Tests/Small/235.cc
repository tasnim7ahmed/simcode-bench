#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lorawan-module.h"

using namespace ns3;

// Define the test suite
class LoRaSensorTestSuite : public TestCase {
public:
    LoRaSensorTestSuite() : TestCase("Test LoRa Sensor Example") {}
    virtual ~LoRaSensorTestSuite() {}

    void DoRun() override {
        TestNodeCreation();
        TestLoRaWANSetup();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestSocketCreation();
        TestPacketTransmission();
        TestPacketReception();
    }

private:
    uint16_t port = 6000;

    // Test if nodes (sensor and sink) are created properly
    void TestNodeCreation() {
        NodeContainer nodes;
        nodes.Create(2);

        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Node creation failed. Expected 2 nodes.");
    }

    // Test if LoRaWAN setup is correct
    void TestLoRaWANSetup() {
        NodeContainer nodes;
        nodes.Create(2);

        LoraHelper lora;
        LoraPhyHelper phy = LoraPhyHelper::Default();
        LorawanMacHelper mac;
        LorawanHelper lorawan;

        NetDeviceContainer devices = lorawan.Install(lora, phy, mac, nodes);

        NS_TEST_ASSERT_MSG_GT(devices.GetN(), 0, "LoRaWAN setup failed. No devices installed.");
    }

    // Test if the Internet stack is installed correctly
    void TestInternetStackInstallation() {
        NodeContainer nodes;
        nodes.Create(2);

        InternetStackHelper internet;
        internet.Install(nodes);

        for (uint32_t i = 0; i < nodes.GetN(); i++) {
            Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NOT_NULL(ipv4, "Internet stack installation failed.");
        }
    }

    // Test if IP addresses are assigned correctly
    void TestIpAddressAssignment() {
        NodeContainer nodes;
        nodes.Create(2);

        LoraHelper lora;
        LoraPhyHelper phy = LoraPhyHelper::Default();
        LorawanMacHelper mac;
        LorawanHelper lorawan;

        NetDeviceContainer devices = lorawan.Install(lora, phy, mac, nodes);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.2.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        NS_TEST_ASSERT_MSG_GT(interfaces.GetN(), 0, "IP address assignment failed.");
    }

    // Test if UDP sockets are created for sensor and sink
    void TestSocketCreation() {
        NodeContainer nodes;
        nodes.Create(2);

        Ptr<Socket> sensorSocket = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());
        NS_TEST_ASSERT_MSG_NOT_NULL(sensorSocket, "UDP sensor socket creation failed.");

        Ptr<Socket> sinkSocket = Socket::CreateSocket(nodes.Get(1), UdpSocketFactory::GetTypeId());
        NS_TEST_ASSERT_MSG_NOT_NULL(sinkSocket, "UDP sink socket creation failed.");
    }

    // Test if the sensor transmits packets correctly
    void TestPacketTransmission() {
        NodeContainer nodes;
        nodes.Create(2);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ptr<Socket> sensorSocket = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());

        InetSocketAddress sinkAddress(Ipv4Address("192.168.2.2"), port);
        Simulator::Schedule(Seconds(1.0), &Socket::SendTo, sensorSocket, Create<Packet>(64), 0, sinkAddress);

        Simulator::Stop(Seconds(3.0));
        Simulator::Run();
        Simulator::Destroy();

        NS_LOG_INFO("Packet transmission test passed.");
    }

    // Test if the sink receives packets from the sensor
    void TestPacketReception() {
        NodeContainer nodes;
        nodes.Create(2);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ptr<Socket> sinkSocket = Socket::CreateSocket(nodes.Get(1), UdpSocketFactory::GetTypeId());
        sinkSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
        sinkSocket->SetRecvCallback([](Ptr<Socket> socket) {
            while (socket->Recv()) {
                NS_LOG_INFO("Sink received data from sensor!");
            }
        });

        Simulator::Stop(Seconds(5.0));
        Simulator::Run();
        Simulator::Destroy();

        NS_LOG_INFO("Packet reception test passed.");
    }
};

// Instantiate the test suite
static LoRaSensorTestSuite loRaSensorTestSuite;
