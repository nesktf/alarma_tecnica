const gridContent = document.getElementById("gridContent");
const templGridRow = document.getElementById("templGridRow").innerHTML;

const lightColor = {
  off: {color: "#A0A0A0", str: "Desconectado"},
  bad: {color: "#CF0000", str: "Movimiento detectado"},
  good: {color: "#00CF00", str: "Normal"},
};

const items = [
  ["Secretaría", lightColor.bad],
  ["Biblioteca", lightColor.good],
  ["Laboratorio 1", lightColor.good],
  ["Laboratorio 2", lightColor.off],
  ["Laboratorio 3", lightColor.good],
  ["Taller", lightColor.bad],
];

let outHtml = "";
const rowElems = 2;
for (let i = 0; i < items.length; i += rowElems) {
  outHtml += Mustache.render(templGridRow, {
    elems: items.slice(i, i+rowElems).map((item) => {
      const [title, status] = item;
      return { title, statusColor: status.color, statusStr: status.str };
    }),
  });
}
gridContent.innerHTML = outHtml;
