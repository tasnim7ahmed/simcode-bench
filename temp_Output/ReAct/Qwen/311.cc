#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    double simTime = 10.0;
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));
    Config::SetDefault("ns3::RvBatteryModel::EnergyDepletionCallback", BooleanValue(true));

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetAttribute("UseRlcSm", BooleanValue(true));
    lteHelper->SetSchedulerType("ns3::PfFfMacScheduler");

    NodeContainer enbNodes;
    enbNodes.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(2);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    NetDeviceContainer enbDevs;
    enbDevs = lteHelper->InstallEnbDevice(enbNodes);

    NetDeviceContainer ueDevs;
    ueDevs = lteHelper->InstallUeDevice(ueNodes);

    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    InternetStackHelper internet;
    internet.Install(ueNodes);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("192.168.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = ipv4h.Assign(ueDevs);

    uint16_t dlPort = 8080;

    // TCP Server (UE 0)
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), dlPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(ueNodes.Get(0));
    sinkApps.Start(Seconds(1.0));
    sinkApps.Stop(Seconds(simTime));

    // TCP Client (UE 1)
    BulkSendHelper bulkSend("ns3::TcpSocketFactory", InetSocketAddress(ueIpIfaces.GetAddress(0), dlPort));
    bulkSend.SetAttribute("MaxBytes", UintegerValue(1000000));
    ApplicationContainer clientApp = bulkSend.Install(ueNodes.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}