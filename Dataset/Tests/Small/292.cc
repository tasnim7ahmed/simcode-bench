#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/epc-helper.h"
#include "ns3/applications-module.h"
#include "ns3/nr-module.h"
#include "ns3/test.h"

using namespace ns3;

class Nr5GExampleTest : public TestCase
{
public:
    Nr5GExampleTest() : TestCase("Test 5G NR Example") {}

    virtual void DoRun() override
    {
        TestNrStackInstallation();
        TestMobilityConfiguration();
        TestIpv4AddressAssignment();
        TestUdpApplications();
        TestSimulation();
    }

private:
    void TestNrStackInstallation()
    {
        // Test NR stack installation on gNB and UE nodes
        NodeContainer gNbNode, ueNode;
        gNbNode.Create(1);
        ueNode.Create(1);

        Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
        nrHelper->Install(gNbNode);
        nrHelper->Install(ueNode);

        // Verify if NR devices were installed on nodes
        Ptr<NetDevice> gNbDevice = gNbNode.Get(0)->GetDevice(0);
        Ptr<NetDevice> ueDevice = ueNode.Get(0)->GetDevice(0);

        NS_TEST_ASSERT_MSG_EQ(gNbDevice != nullptr, true, "gNB NR device installation failed");
        NS_TEST_ASSERT_MSG_EQ(ueDevice != nullptr, true, "UE NR device installation failed");
    }

    void TestMobilityConfiguration()
    {
        // Test mobility configuration for gNB (fixed) and UE (mobile)
        NodeContainer gNbNode, ueNode;
        gNbNode.Create(1);
        ueNode.Create(1);

        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(gNbNode);
        mobility.Install(ueNode);

        // Check if mobility models are installed correctly
        Ptr<MobilityModel> gNbMobility = gNbNode.Get(0)->GetObject<MobilityModel>();
        Ptr<MobilityModel> ueMobility = ueNode.Get(0)->GetObject<MobilityModel>();

        NS_TEST_ASSERT_MSG_EQ(gNbMobility != nullptr, true, "gNB mobility model installation failed");
        NS_TEST_ASSERT_MSG_EQ(ueMobility != nullptr, true, "UE mobility model installation failed");
    }

    void TestIpv4AddressAssignment()
    {
        // Test IP address assignment for UE node
        NodeContainer ueNode;
        ueNode.Create(1);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.0.0.0", "255.255.255.0");
        Ipv4InterfaceContainer ueIpInterface = ipv4.Assign(ueNode);

        // Verify if IP address is assigned correctly
        Ipv4Address ueIp = ueIpInterface.GetAddress(0);

        NS_TEST_ASSERT_MSG_EQ(ueIp.IsValid(), true, "IP address assignment failed for UE node");
    }

    void TestUdpApplications()
    {
        // Test UDP server and client setup
        NodeContainer gNbNode, ueNode;
        gNbNode.Create(1);
        ueNode.Create(1);

        uint16_t port = 8080;

        // Set up UDP server on gNB
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(gNbNode);
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        // Set up UDP client on UE
        Ipv4InterfaceContainer ueIpInterface;
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.0.0.0", "255.255.255.0");
        ueIpInterface = ipv4.Assign(ueNode);

        UdpClientHelper udpClient(ueIpInterface.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(100));
        udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(500)));
        udpClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApp = udpClient.Install(ueNode.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        // Verify if server and client applications are correctly installed
        NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "UDP server application installation failed");
        NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "UDP client application installation failed");
    }

    void TestSimulation()
    {
        // Test if the simulation runs without errors
        NodeContainer gNbNode, ueNode;
        gNbNode.Create(1);
        ueNode.Create(1);

        Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
        nrHelper->Install(gNbNode);
        nrHelper->Install(ueNode);

        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(gNbNode);
        mobility.Install(ueNode);

        InternetStackHelper internet;
        internet.Install(ueNode);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.0.0.0", "255.255.255.0");
        ipv4.Assign(ueNode);

        uint16_t port = 8080;
        UdpServerHelper udpServer(port);
        ApplicationContainer serverApp = udpServer.Install(gNbNode);
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        UdpClientHelper udpClient(ueNode.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetAddress(), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(100));
        udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(500)));
        udpClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApp = udpClient.Install(ueNode.Get(0));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));

        Simulator::Run();
        Simulator::Destroy();

        // Verify if simulation completes successfully
        NS_TEST_ASSERT_MSG_EQ(Simulator::GetContext()->GetTotalSimTime().GetSeconds() > 0, true, "Simulation did not run correctly");
    }
};

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("Nr5GExample", LOG_LEVEL_INFO);

    // Run the unit tests
    Ptr<Nr5GExampleTest> test = CreateObject<Nr5GExampleTest>();
    test->Run();

    return 0;
}
