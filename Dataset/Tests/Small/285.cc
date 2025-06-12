#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include <cassert>

using namespace ns3;

void TestNodeCreation() {
    NodeContainer ueNodes, enbNode;
    ueNodes.Create(3);
    enbNode.Create(1);
    assert(ueNodes.GetN() == 3 && enbNode.GetN() == 1);
    NS_LOG_INFO("Node creation test passed.");
}

void TestMobilityInstallation() {
    NodeContainer ueNodes, enbNode;
    ueNodes.Create(3);
    enbNode.Create(1);
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNode);
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds", RectangleValue(Rectangle(-100, 100, -100, 100)));
    mobility.Install(ueNodes);
    assert(enbNode.Get(0)->GetObject<MobilityModel>() != nullptr);
    assert(ueNodes.Get(0)->GetObject<MobilityModel>() != nullptr);
    NS_LOG_INFO("Mobility model test passed.");
}

void TestLteInstallation() {
    NodeContainer ueNodes, enbNode;
    ueNodes.Create(3);
    enbNode.Create(1);
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    NetDeviceContainer enbDevice = lteHelper->InstallEnbDevice(enbNode);
    NetDeviceContainer ueDevices = lteHelper->InstallUeDevice(ueNodes);
    assert(enbDevice.GetN() == 1);
    assert(ueDevices.GetN() == 3);
    NS_LOG_INFO("LTE installation test passed.");
}

void TestIpAssignment() {
    NodeContainer ueNodes;
    ueNodes.Create(3);
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    NetDeviceContainer ueDevices = lteHelper->InstallUeDevice(ueNodes);
    InternetStackHelper internet;
    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueInterfaces = lteHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevices));
    assert(ueInterfaces.GetN() == 3);
    NS_LOG_INFO("IP assignment test passed.");
}

void TestAttachment() {
    NodeContainer ueNodes, enbNode;
    ueNodes.Create(3);
    enbNode.Create(1);
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    NetDeviceContainer enbDevice = lteHelper->InstallEnbDevice(enbNode);
    NetDeviceContainer ueDevices = lteHelper->InstallUeDevice(ueNodes);
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        lteHelper->Attach(ueDevices.Get(i), enbDevice.Get(0));
    }
    NS_LOG_INFO("UE attachment test passed.");
}

void RunAllTests() {
    TestNodeCreation();
    TestMobilityInstallation();
    TestLteInstallation();
    TestIpAssignment();
    TestAttachment();
    NS_LOG_INFO("All tests passed successfully.");
}

int main() {
    LogComponentEnable("LteBasicSimulation", LOG_LEVEL_INFO);
    RunAllTests();
    return 0;
}
