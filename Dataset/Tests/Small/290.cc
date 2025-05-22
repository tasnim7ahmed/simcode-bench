#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

class LteBasicExampleTest : public TestCase
{
public:
    LteBasicExampleTest() : TestCase("Test LTE Basic Example") {}

    virtual void DoRun() override
    {
        TestLteSetup();
        TestMobilityConfig();
        TestInternetStackInstall();
        TestUdpApplications();
    }

private:
    void TestLteSetup()
    {
        // Test LTE devices and helper creation
        NodeContainer ueNode, enbNode;
        ueNode.Create(1);
        enbNode.Create(1);

        Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
        NS_TEST_ASSERT_MSG_EQ(lteHelper != nullptr, true, "LteHelper creation failed");

        NetDeviceContainer enbDevice = lteHelper->InstallEnbDevice(enbNode);
        NetDeviceContainer ueDevice = lteHelper->InstallUeDevice(ueNode);

        NS_TEST_ASSERT_MSG_EQ(enbDevice.Get(0)->GetObject<LteEnbNetDevice>(), nullptr, "EnbDevice installation failed");
        NS_TEST_ASSERT_MSG_EQ(ueDevice.Get(0)->GetObject<LteUeNetDevice>(), nullptr, "UeDevice installation failed");

        // Attach UE to eNodeB
        lteHelper->Attach(ueDevice, enbDevice.Get(0));
    }

    void TestMobilityConfig()
    {
        // Test mobility models configuration
        NodeContainer ueNode, enbNode;
        ueNode.Create(1);
        enbNode.Create(1);

        MobilityHelper mobility;

        // eNodeB Mobility Model: Constant Position
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(enbNode);
        Ptr<ConstantPositionMobilityModel> enbMobility = enbNode.Get(0)->GetObject<ConstantPositionMobilityModel>();
        NS_TEST_ASSERT_MSG_EQ(enbMobility != nullptr, true, "eNodeB Mobility model not configured correctly");

        // UE Mobility Model: Random Walk 2D
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds", RectangleValue(Rectangle(-100, 100, -100, 100)));
        mobility.Install(ueNode);
        Ptr<RandomWalk2dMobilityModel> ueMobility = ueNode.Get(0)->GetObject<RandomWalk2dMobilityModel>();
        NS_TEST_ASSERT_MSG_EQ(ueMobility != nullptr, true, "UE Mobility model not configured correctly");
    }

    void TestInternetStackInstall()
    {
        // Test Internet Stack installation and IP assignment
        NodeContainer ueNode;
        ueNode.Create(1);

        InternetStackHelper internet;
        internet.Install(ueNode);

        Ptr<Ipv4> ipv4 = ueNode.Get(0)->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_EQ(ipv4 != nullptr, true, "Internet stack installation failed");

        // Check if IP address assignment works
        Ipv4InterfaceContainer ueIpIface;
        ueIpIface.Add(ipv4->GetAddress(1, 0));
        NS_TEST_ASSERT_MSG_EQ(ueIpIface.GetAddress(0) != Ipv4Address("0.0.0.0"), true, "IP Address not assigned correctly");
    }

    void TestUdpApplications()
    {
        // Test UDP server and client setup
        NodeContainer ueNode, enbNode;
        ueNode.Create(1);
        enbNode.Create(1);

        // UDP Server
        uint16_t port = 5000;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(ueNode.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // UDP Client
        UdpClientHelper udpClient(ueNode.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetAddress(), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(100));
        udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(200)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = udpClient.Install(enbNode.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Test UDP application running by verifying the start and stop time
        NS_TEST_ASSERT_MSG_EQ(serverApp.Get(0)->GetStartTime(), Seconds(1.0), "UDP server start time mismatch");
        NS_TEST_ASSERT_MSG_EQ(clientApp.Get(0)->GetStopTime(), Seconds(10.0), "UDP client stop time mismatch");
    }
};

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("LteBasicExample", LOG_LEVEL_INFO);

    // Run the unit tests
    Ptr<LteBasicExampleTest> test = CreateObject<LteBasicExampleTest>();
    test->Run();

    return 0;
}
