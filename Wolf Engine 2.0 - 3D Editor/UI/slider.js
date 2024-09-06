var template = document.createElement('template');
template.innerHTML = `
    <section class="range-slider">
    </section>
`;

class WolfSlider extends HTMLElement {
    constructor() {
        super();
        this.appendChild(template.content.cloneNode(true));
        this.min = this.getAttribute('min')?this.getAttribute('min'):"0";
        this.max = this.getAttribute('max')?this.getAttribute('max'):"100";
        this.step = this.getAttribute('step')?this.getAttribute('step'): (this.min + this.max)/100;
        this.values = this.getAttribute("value")? this.getAttribute("value").split(";").map((str)=>Number(str)) : [(this.min + this.max)/2];
        this.disabled = this.getAttribute("disabled")? this.getAttribute("disabled") : false;
        this.noInput = this.getAttribute("noInput")? this.getAttribute("noInput") : false;
        var div = document.createElement('input');
        if (this.noInput) 
            div.style = "display: none";
        div.value =`${this.values}`;
        div.classList.add("textInput");
        div.classList.add("numberInput");
        let oninputName = this.getAttribute('oninput');
        div.addEventListener('change', function(ev) { 
            eval(oninputName + "(this.value)");
            this.parentNode.querySelector("input").value = this.value;
        });
        this.appendChild(div)

        this.values.forEach((val, index) => {
            this.querySelector('section').appendChild(this.input({
                type: "range",
                value: val,
                key: index,
                ariaValueNow: val,
                ariaValueText: val,
                ariaOrientation: "horizontal",
                ariaValueMax: this.max,
                ariaValueMin: this.min,
                min: this.min,
                max: this.max,
                step:this.step
            }))
        })
    }

    input({ key, value, ...props }) {
        this.el = document.createElement("input");
        Object.assign(this.el, props);
        this.el.setAttribute("value", value);
        this.el.addEventListener('input', () => {
        this.values[key] = Number(this.el.value);
        this.querySelector('.textInput').value =`${this.values}`;
        window[this.getAttribute('oninput')](this.values.length == 1 ? this.values[0] : this.values);
        })
        this.el.disabled = this.disabled;
        return this.el;
    }

    toggleDisabled() {
        this.el.disabled = !this.el.disabled;
    }

    disconnectedCallback() {
        //this.removeEventListener('change');
    }
}

customElements.define("wolf-slider", WolfSlider);