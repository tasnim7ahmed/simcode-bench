#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/mmwave-helper.h"

using namespace ns3;

class MmWaveTestSuite : public TestCase
{
public:
    MmWaveTestSuite() : TestCase("Test mmWave Echo Application Setup") {}
    virtual ~MmWaveTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestMobilityConfiguration();
        TestMmWaveDeviceInstallation();
        TestIpAddressAssignment();
        TestApplicationSetup();
    }

private:
    // Test the creation of gNB and UE nodes
    void TestNodeCreation()
    {
        NodeContainer gNbNodes, ueNodes;
        gNbNodes.Create(1); // One gNB (Base Station)
        ueNodes.Create(2);  // Two UEs (User Equipment)

        // Ensure the correct number of nodes are created
        NS_TEST_ASSERT_MSG_EQ(gNbNodes.GetN(), 1, "Failed to create 1 gNB node.");
        NS_TEST_ASSERT_MSG_EQ(ueNodes.GetN(), 2, "Failed to create 2 UE nodes.");
    }

    // Test the mobility configuration of the gNB and UEs
    void TestMobilityConfiguration()
    {
        NodeContainer gNbNodes, ueNodes;
        gNbNodes.Create(1);
        ueNodes.Create(2);

        MobilityHelper mobility;

        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(gNbNodes); // gNB as constant position

        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                  "Bounds", RectangleValue(Rectangle(-10, 10, -10, 10)));
        mobility.Install(ueNodes); // UEs as random walk

        Ptr<MobilityModel> gNbMobility = gNbNodes.Get(0)->GetObject<MobilityModel>();
        Ptr<MobilityModel> ue1Mobility = ueNodes.Get(0)->GetObject<MobilityModel>();
        Ptr<MobilityModel> ue2Mobility = ueNodes.Get(1)->GetObject<MobilityModel>();

        // Ensure that the mobility models are correctly set
        NS_TEST_ASSERT_MSG_EQ(gNbMobility->GetPosition(), Vector(0, 0, 0), "Failed to set position for gNB.");
        NS_TEST_ASSERT_MSG_NE(ue1Mobility->GetPosition(), Vector(0, 0, 0), "Failed to set random walk position for UE 1.");
        NS_TEST_ASSERT_MSG_NE(ue2Mobility->GetPosition(), Vector(0, 0, 0), "Failed to set random walk position for UE 2.");
    }

    // Test the installation of mmWave devices on the nodes
    void TestMmWaveDeviceInstallation()
    {
        NodeContainer gNbNodes, ueNodes;
        gNbNodes.Create(1);
        ueNodes.Create(2);

        Ptr<MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper>();

        NetDeviceContainer gNbDevices = mmwaveHelper->InstallEnbDevice(gNbNodes);
        NetDeviceContainer ueDevices = mmwaveHelper->InstallUeDevice(ueNodes);

        // Ensure mmWave devices are installed on gNB and UE nodes
        NS_TEST_ASSERT_MSG_EQ(gNbDevices.GetN(), 1, "Failed to install mmWave device on gNB.");
        NS_TEST_ASSERT_MSG_EQ(ueDevices.GetN(), 2, "Failed to install mmWave devices on UEs.");
    }

    // Test the assignment of IP addresses to the UEs
    void TestIpAddressAssignment()
    {
        NodeContainer gNbNodes, ueNodes;
        gNbNodes.Create(1);
        ueNodes.Create(2);

        Ptr<MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper>();

        NetDeviceContainer gNbDevices = mmwaveHelper->InstallEnbDevice(gNbNodes);
        NetDeviceContainer ueDevices = mmwaveHelper->InstallUeDevice(ueNodes);

        InternetStackHelper internet;
        internet.Install(ueNodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer ueInterfaces = address.Assign(ueDevices);

        // Ensure that the IP addresses are assigned to the UEs
        Ipv4Address ipNode1 = ueInterfaces.GetAddress(0);
        Ipv4Address ipNode2 = ueInterfaces.GetAddress(1);

        NS_TEST_ASSERT_MSG_NE(ipNode1, Ipv4Address("0.0.0.0"), "Failed to assign IP address to UE 1.");
        NS_TEST_ASSERT_MSG_NE(ipNode2, Ipv4Address("0.0.0.0"), "Failed to assign IP address to UE 2.");
    }

    // Test the setup of UDP Echo Server and Client applications
    void TestApplicationSetup()
    {
        NodeContainer gNbNodes, ueNodes;
        gNbNodes.Create(1);
        ueNodes.Create(2);

        Ptr<MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper>();

        NetDeviceContainer gNbDevices = mmwaveHelper->InstallEnbDevice(gNbNodes);
        NetDeviceContainer ueDevices = mmwaveHelper->InstallUeDevice(ueNodes);

        InternetStackHelper internet;
        internet.Install(ueNodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer ueInterfaces = address.Assign(ueDevices);

        mmwaveHelper->Attach(ueDevices, gNbDevices.Get(0));

        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApps = echoServer.Install(ueNodes.Get(1)); // Second UE as server
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));

        UdpEchoClientHelper echoClient(ueInterfaces.GetAddress(1), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(5));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApps = echoClient.Install(ueNodes.Get(0)); // First UE as client
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));

        // Ensure that the server and client applications are correctly installed
        NS_TEST_ASSERT_MSG_EQ(serverApps.GetN(), 1, "Failed to install UDP Echo Server on UE 2.");
        NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), 1, "Failed to install UDP Echo Client on UE 1.");
    }
};

// Instantiate the test suite
static MmWaveTestSuite mmWaveTestSuite;
