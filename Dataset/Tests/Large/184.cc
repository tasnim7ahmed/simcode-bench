#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

// Test for node creation: eNodeB, UEs, and Remote Host
void TestNodeCreation()
{
    NodeContainer ueNodes;
    ueNodes.Create(2);
    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);

    NS_TEST_ASSERT_MSG_EQ(ueNodes.GetN(), 2, "Failed to create 2 UE nodes");
    NS_TEST_ASSERT_MSG_EQ(enbNodes.GetN(), 1, "Failed to create 1 eNodeB node");
    NS_TEST_ASSERT_MSG_EQ(remoteHostContainer.GetN(), 1, "Failed to create 1 remote host");
}

// Test for eNodeB LTE device installation
void TestEnbDeviceInstallation()
{
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    NodeContainer enbNodes;
    enbNodes.Create(1);
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);

    NS_TEST_ASSERT_MSG_EQ(enbLteDevs.GetN(), 1, "Failed to install eNodeB device");
}

// Test for UE LTE device installation
void TestUeDeviceInstallation()
{
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    NodeContainer ueNodes;
    ueNodes.Create(2);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    NS_TEST_ASSERT_MSG_EQ(ueLteDevs.GetN(), 2, "Failed to install UE devices");
}

// Test for mobility model installation
void TestMobilityInstallation()
{
    NodeContainer ueNodes;
    ueNodes.Create(2);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    // Ensure that the nodes are correctly installed
    Ptr<MobilityModel> enbMobility = enbNodes.Get(0)->GetObject<MobilityModel>();
    Ptr<MobilityModel> ueMobility1 = ueNodes.Get(0)->GetObject<MobilityModel>();
    Ptr<MobilityModel> ueMobility2 = ueNodes.Get(1)->GetObject<MobilityModel>();

    NS_TEST_ASSERT_MSG_NOT_NULL(enbMobility, "eNodeB mobility model is not installed");
    NS_TEST_ASSERT_MSG_NOT_NULL(ueMobility1, "UE 1 mobility model is not installed");
    NS_TEST_ASSERT_MSG_NOT_NULL(ueMobility2, "UE 2 mobility model is not installed");
}

// Test for IP address assignment to UE nodes
void TestIpAddressAssignment()
{
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    NodeContainer ueNodes;
    ueNodes.Create(2);
    NetDeviceContainer ueLteDevs = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));
    
    NS_TEST_ASSERT_MSG_EQ(ueIpIfaces.GetN(), 2, "Failed to assign IP addresses to UE nodes");
}

// Test for application installation (Client and Server)
void TestApplicationInstallation()
{
    NodeContainer ueNodes;
    ueNodes.Create(2);
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    uint16_t dlPort = 1234;
    UdpClientHelper dlClient(Ipv4Address("1.0.0.2"), dlPort);
    ApplicationContainer clientApps;
    clientApps.Add(dlClient.Install(ueNodes.Get(0)));
    clientApps.Add(dlClient.Install(ueNodes.Get(1)));

    uint16_t ulPort = 2000;
    UdpServerHelper ulServer(ulPort);
    ApplicationContainer serverApps = ulServer.Install(remoteHost);

    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), 2, "Failed to install client applications");
    NS_TEST_ASSERT_MSG_EQ(serverApps.GetN(), 1, "Failed to install server application");
}

// Test for the correct attachment of UEs to eNodeB
void TestUeAttachment()
{
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    NodeContainer ueNodes;
    ueNodes.Create(2);
    NodeContainer enbNodes;
    enbNodes.Create(1);
    
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);
    
    lteHelper->Attach(ueLteDevs, enbLteDevs.Get(0));

    NS_TEST_ASSERT_MSG_EQ(lteHelper->IsUeAttached(ueLteDevs.Get(0)), true, "UE 1 is not attached to eNodeB");
    NS_TEST_ASSERT_MSG_EQ(lteHelper->IsUeAttached(ueLteDevs.Get(1)), true, "UE 2 is not attached to eNodeB");
}

// Test for NetAnim visualization setup
void TestNetAnimSetup()
{
    NodeContainer ueNodes;
    ueNodes.Create(2);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    AnimationInterface anim("lte_netanim.xml");
    anim.SetConstantPosition(enbNodes.Get(0), 50.0, 50.0); // eNodeB
    anim.SetConstantPosition(ueNodes.Get(0), 60.0, 50.0);  // UE 1
    anim.SetConstantPosition(ueNodes.Get(1), 70.0, 50.0);  // UE 2

    anim.EnablePacketMetadata(true);  // Enable packet tracking

    // Validate positions in the animation
    NS_TEST_ASSERT_MSG_EQ(anim.GetNodePosition(enbNodes.Get(0)).x, 50.0, "eNodeB position incorrect");
    NS_TEST_ASSERT_MSG_EQ(anim.GetNodePosition(ueNodes.Get(0)).x, 60.0, "UE 1 position incorrect");
    NS_TEST_ASSERT_MSG_EQ(anim.GetNodePosition(ueNodes.Get(1)).x, 70.0, "UE 2 position incorrect");
}

int main() {
    TestNodeCreation();
    TestEnbDeviceInstallation();
    TestUeDeviceInstallation();
    TestMobilityInstallation();
    TestIpAddressAssignment();
    TestApplicationInstallation();
    TestUeAttachment();
    TestNetAnimSetup();

    return 0;
}
