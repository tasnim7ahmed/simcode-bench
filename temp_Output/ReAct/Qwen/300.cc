#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/wifi-module.h"
#include "ns3/netanim-module.h"
#include "ns3/zigbee-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ZigbeeSensorNetwork");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 10;
    uint32_t packetSize = 128;
    Time interval = MilliSeconds(500);
    double simulationTime = 10.0;

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    Ptr<ZigbeeHelper> zigbeeHelper = CreateObject<ZigbeeHelper>();
    zigbeeHelper->SetMode("ZIGBEE_PRO");
    zigbeeHelper->AssignPanId(nodes);

    NetDeviceContainer devices = zigbeeHelper->Install(nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(10),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    uint16_t coordinatorPort = 9;
    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < numNodes; ++i) {
        if (i == 0) {
            UdpServerHelper server(coordinatorPort);
            serverApps.Add(server.Install(nodes.Get(i)));
            serverApps.Start(Seconds(0.0));
            serverApps.Stop(Seconds(simulationTime));
        } else {
            UdpClientHelper client(interfaces.GetAddress(0), coordinatorPort);
            client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
            client.SetAttribute("Interval", TimeValue(interval));
            client.SetAttribute("PacketSize", UintegerValue(packetSize));

            ApplicationContainer clientApp = client.Install(nodes.Get(i));
            clientApp.Start(Seconds(0.1));
            clientApp.Stop(Seconds(simulationTime));
        }
    }

    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("zigbee-wsn.tr");
    zigbeeHelper->EnableAsciiAll(stream);

    zigbeeHelper->EnablePcapAll("zigbee-wsn");

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}