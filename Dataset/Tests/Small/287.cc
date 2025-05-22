#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mmwave-helper.h"
#include "ns3/test.h"

using namespace ns3;

class NrMmWaveTestSuite : public TestSuite
{
public:
    NrMmWaveTestSuite() : TestSuite("nr-mmwave-tests", UNIT)
    {
        AddTestCase(new TestNodeCreation, TestCase::QUICK);
        AddTestCase(new TestMobilityInstallation, TestCase::QUICK);
        AddTestCase(new TestMmWaveInstallation, TestCase::QUICK);
        AddTestCase(new TestIpAssignment, TestCase::QUICK);
        AddTestCase(new TestUeAttachment, TestCase::QUICK);
        AddTestCase(new TestUdpCommunication, TestCase::EXTENSIVE);
    }
};

// Test if nodes are created correctly
class TestNodeCreation : public TestCase
{
public:
    TestNodeCreation() : TestCase("Test Node Creation") {}

    virtual void DoRun()
    {
        NodeContainer ueNodes, gnbNode;
        ueNodes.Create(2);
        gnbNode.Create(1);

        NS_TEST_ASSERT_MSG_EQ(ueNodes.GetN(), 2, "UE nodes should be 2");
        NS_TEST_ASSERT_MSG_EQ(gnbNode.GetN(), 1, "gNB node should be 1");
    }
};

// Test mobility installation
class TestMobilityInstallation : public TestCase
{
public:
    TestMobilityInstallation() : TestCase("Test Mobility Installation") {}

    virtual void DoRun()
    {
        NodeContainer ueNodes, gnbNode;
        ueNodes.Create(2);
        gnbNode.Create(1);

        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(gnbNode);

        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                  "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
        mobility.Install(ueNodes);

        Ptr<MobilityModel> gnbMobility = gnbNode.Get(0)->GetObject<MobilityModel>();
        Ptr<MobilityModel> ueMobility = ueNodes.Get(0)->GetObject<MobilityModel>();

        NS_TEST_ASSERT_MSG_NE(gnbMobility, nullptr, "gNB should have a mobility model");
        NS_TEST_ASSERT_MSG_NE(ueMobility, nullptr, "UE should have a mobility model");
    }
};

// Test mmWave module installation
class TestMmWaveInstallation : public TestCase
{
public:
    TestMmWaveInstallation() : TestCase("Test MmWave Installation") {}

    virtual void DoRun()
    {
        NodeContainer ueNodes, gnbNode;
        ueNodes.Create(2);
        gnbNode.Create(1);

        Ptr<MmWaveHelper> mmWaveHelper = CreateObject<MmWaveHelper>();
        NetDeviceContainer gnbDevice = mmWaveHelper->InstallGnbDevice(gnbNode);
        NetDeviceContainer ueDevices = mmWaveHelper->InstallUeDevice(ueNodes);

        NS_TEST_ASSERT_MSG_GT(gnbDevice.GetN(), 0, "gNB device should be installed");
        NS_TEST_ASSERT_MSG_GT(ueDevices.GetN(), 0, "UE devices should be installed");
    }
};

// Test IP address assignment
class TestIpAssignment : public TestCase
{
public:
    TestIpAssignment() : TestCase("Test IP Address Assignment") {}

    virtual void DoRun()
    {
        NodeContainer ueNodes;
        ueNodes.Create(2);

        Ptr<MmWaveHelper> mmWaveHelper = CreateObject<MmWaveHelper>();
        NetDeviceContainer ueDevices = mmWaveHelper->InstallUeDevice(ueNodes);

        InternetStackHelper internet;
        internet.Install(ueNodes);

        Ipv4InterfaceContainer ueInterfaces = mmWaveHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevices));

        NS_TEST_ASSERT_MSG_GT(ueInterfaces.GetN(), 0, "UE interfaces should have IP addresses");
    }
};

// Test UE attachment to gNB
class TestUeAttachment : public TestCase
{
public:
    TestUeAttachment() : TestCase("Test UE Attachment to gNB") {}

    virtual void DoRun()
    {
        NodeContainer ueNodes, gnbNode;
        ueNodes.Create(2);
        gnbNode.Create(1);

        Ptr<MmWaveHelper> mmWaveHelper = CreateObject<MmWaveHelper>();
        NetDeviceContainer gnbDevice = mmWaveHelper->InstallGnbDevice(gnbNode);
        NetDeviceContainer ueDevices = mmWaveHelper->InstallUeDevice(ueNodes);

        for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
        {
            mmWaveHelper->Attach(ueDevices.Get(i), gnbDevice.Get(0));
        }

        NS_TEST_ASSERT_MSG_NE(ueDevices.Get(0)->GetObject<MmWaveNetDevice>()->GetRrc(), nullptr, "UE should be attached to gNB");
    }
};

// Test UDP communication
class TestUdpCommunication : public TestCase
{
public:
    TestUdpCommunication() : TestCase("Test UDP Communication") {}

    virtual void DoRun()
    {
        NodeContainer ueNodes;
        ueNodes.Create(2);

        InternetStackHelper internet;
        internet.Install(ueNodes);

        Ptr<MmWaveHelper> mmWaveHelper = CreateObject<MmWaveHelper>();
        NetDeviceContainer ueDevices = mmWaveHelper->InstallUeDevice(ueNodes);
        Ipv4InterfaceContainer ueInterfaces = mmWaveHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevices));

        uint16_t port = 5001;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(ueNodes.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        UdpClientHelper udpClient(ueInterfaces.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = udpClient.Install(ueNodes.Get(1));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        Simulator::Run();

        Ptr<UdpServer> server = DynamicCast<UdpServer>(serverApp.Get(0));
        NS_TEST_ASSERT_MSG_GT(server->GetReceived(), 0, "UDP packets should be received");

        Simulator::Destroy();
    }
};

static NrMmWaveTestSuite nrMmWaveTestSuite;
