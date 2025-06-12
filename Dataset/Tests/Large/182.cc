#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/test.h"  // Include the ns3 Test module for unit testing

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteSimulationExampleTest");

// Test the creation of LTE nodes (eNodeB and UEs)
void TestNodeCreation() {
    NodeContainer ueNodes;
    ueNodes.Create(2);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    NS_TEST_ASSERT_MSG_EQ(ueNodes.GetN(), 2, "Incorrect number of UE nodes created");
    NS_TEST_ASSERT_MSG_EQ(enbNodes.GetN(), 1, "Incorrect number of eNodeB nodes created");
}

// Test LTE device installation (eNodeB and UEs)
void TestLteDeviceInstallation() {
    NodeContainer ueNodes;
    ueNodes.Create(2);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Verify LTE devices installation
    NS_TEST_ASSERT_MSG_EQ(enbLteDevs.GetN(), 1, "Incorrect number of eNodeB LTE devices installed");
    NS_TEST_ASSERT_MSG_EQ(ueLteDevs.GetN(), 2, "Incorrect number of UE LTE devices installed");
}

// Test mobility setup (eNodeB and UEs)
void TestMobilitySetup() {
    NodeContainer ueNodes;
    ueNodes.Create(2);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    Ptr<ConstantPositionMobilityModel> enbMobility = enbNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
    Ptr<ConstantPositionMobilityModel> ueMobility1 = ueNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
    Ptr<ConstantPositionMobilityModel> ueMobility2 = ueNodes.Get(1)->GetObject<ConstantPositionMobilityModel>();

    // Verify mobility models for eNodeB and UEs
    NS_TEST_ASSERT_MSG_NE(enbMobility, nullptr, "eNodeB mobility model not installed correctly");
    NS_TEST_ASSERT_MSG_NE(ueMobility1, nullptr, "UE1 mobility model not installed correctly");
    NS_TEST_ASSERT_MSG_NE(ueMobility2, nullptr, "UE2 mobility model not installed correctly");
}

// Test IP address assignment for UEs and eNodeB
void TestIpAddressAssignment() {
    NodeContainer ueNodes;
    ueNodes.Create(2);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    InternetStackHelper internet;
    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpInterfaces = epcHelper->AssignUeIpv4Address(ueLteDevs);

    // Verify IP address assignment
    NS_TEST_ASSERT_MSG_NE(ueIpInterfaces.GetAddress(0), Ipv4Address("0.0.0.0"), "UE1 IP address not assigned correctly");
    NS_TEST_ASSERT_MSG_NE(ueIpInterfaces.GetAddress(1), Ipv4Address("0.0.0.0"), "UE2 IP address not assigned correctly");
}

// Test application installation (OnOff and PacketSink)
void TestApplicationInstallation() {
    NodeContainer ueNodes;
    ueNodes.Create(2);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    uint16_t dlPort = 1234;
    OnOffHelper onoff("ns3::TcpSocketFactory", Address(InetSocketAddress(ueLteDevs.GetAddress(1), dlPort)));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("DataRate", StringValue("50Mbps"));
    ApplicationContainer clientApps = onoff.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::TcpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), dlPort)));
    ApplicationContainer serverApps = sink.Install(ueNodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Verify application installation
    NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), 1, "OnOff application not installed correctly");
    NS_TEST_ASSERT_MSG_EQ(serverApps.GetN(), 1, "PacketSink application not installed correctly");
}

// Test NetAnim visualization setup
void TestNetAnimVisualization() {
    NodeContainer ueNodes;
    ueNodes.Create(2);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    AnimationInterface anim("lte_netanim_test.xml");

    anim.SetConstantPosition(enbNodes.Get(0), 0.0, 0.0);   // eNodeB at origin
    anim.SetConstantPosition(ueNodes.Get(0), 10.0, 0.0);   // UE 1 at 10m
    anim.SetConstantPosition(ueNodes.Get(1), 20.0, 0.0);   // UE 2 at 20m

    anim.EnablePacketMetadata(true);

    // Verify NetAnim file creation and positions
    NS_TEST_ASSERT_MSG_EQ(anim.GetFileName(), "lte_netanim_test.xml", "NetAnim file was not created correctly");
    NS_TEST_ASSERT_MSG_EQ(anim.GetNodePosition(enbNodes.Get(0)).x, 0.0, "eNodeB position was not set correctly in NetAnim");
    NS_TEST_ASSERT_MSG_EQ(anim.GetNodePosition(ueNodes.Get(0)).x, 10.0, "UE1 position was not set correctly in NetAnim");
    NS_TEST_ASSERT_MSG_EQ(anim.GetNodePosition(ueNodes.Get(1)).x, 20.0, "UE2 position was not set correctly in NetAnim");
}

// Main function to run all the unit tests
int main(int argc, char *argv[]) {
    // Run the unit tests
    TestNodeCreation();
    TestLteDeviceInstallation();
    TestMobilitySetup();
    TestIpAddressAssignment();
    TestApplicationInstallation();
    TestNetAnimVisualization();

    return 0;
}
