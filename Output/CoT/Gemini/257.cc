#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Config::SetDefault ("ns3::LteUePhy::EnableUplinkPowerControl", BooleanValue (false));

    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(2);

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(1500));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    NetDeviceContainer epcEnbDevs;
    NetDeviceContainer epcUeDevs;
    epcHelper->CreateEpcNetwork("10.0.0.0", PrefixLength{24});

    for (size_t i = 0; i < enbNodes.GetN(); ++i) {
        NetDeviceContainer enb_epc = epcHelper->InstallEnbDevice(enbNodes.Get(i));
        epcEnbDevs.Add(enb_epc);
    }

    InternetStackHelper internet;
    internet.Install(enbNodes);
    internet.Install(ueNodes);

    Ipv4InterfaceContainer enbIpIface = epcHelper->AssignEnbIpv4Address(NetDeviceContainer(epcEnbDevs));
    Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address(ueDevs);

    for (int i = 0; i < ueNodes.GetN(); i++) {
        lteHelper->Attach(ueDevs.Get(i), enbDevs.Get(0));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::GridMobilityModel",
                              "LayoutType", new EnumValue(GridPositionAllocator::ROW_MAJOR),
                              "XStep", new DoubleValue(5.0),
                              "YStep", new DoubleValue(10.0));
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", new DoubleValue(0.0),
                                  "MinY", new DoubleValue(0.0),
                                  "DeltaX", new DoubleValue(5.0),
                                  "DeltaY", new DoubleValue(10.0),
                                  "GridWidth", new UintegerValue(3),
                                  "LayoutType", new EnumValue(GridPositionAllocator::ROW_MAJOR));

    mobility.Install(ueNodes);
    mobility.Install(enbNodes);

    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(ueNodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpClientHelper client(ueIpIface.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(2));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApp = client.Install(ueNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}